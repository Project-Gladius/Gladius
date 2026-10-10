#include "Scripts/Temple/TempleAIBatchClient.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString UTempleAIBatchClient::SendBatch(const TArray<FTempleTalkBatchMessage>& Messages, const FDateTime& WindowStartedAt, const FDateTime& WindowEndedAt)
{
	if (Messages.IsEmpty())
	{
		return FString();
	}

	CancelActiveBatch();

	ActiveBatch = FActiveBatch();
	ActiveBatch.Messages = Messages;
	ActiveBatch.WindowStartedAt = WindowStartedAt;
	ActiveBatch.WindowEndedAt = WindowEndedAt;
	ActiveBatch.BatchId = FString::Printf(
		TEXT("temple-%s-%04lld"),
		*WindowEndedAt.ToString(TEXT("%Y%m%dT%H%M%SZ")),
		++BatchSequence
	);

	SendActiveRequest();
	return ActiveBatch.BatchId;
}

void UTempleAIBatchClient::CancelActiveBatch()
{
	if (ActiveRequest.IsValid())
	{
		//취소된 요청의 콜백이 들어와도 무시되도록 먼저 참조를 끊음
		FHttpRequestPtr CancelledRequest = ActiveRequest;
		ActiveRequest.Reset();
		CancelledRequest->OnProcessRequestComplete().Unbind();
		CancelledRequest->CancelRequest();
	}
}

void UTempleAIBatchClient::SendActiveRequest()
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schemaVersion"), 1);
	Root->SetStringField(TEXT("batchId"), ActiveBatch.BatchId);
	Root->SetStringField(TEXT("windowStartedAt"), ActiveBatch.WindowStartedAt.ToIso8601());
	Root->SetStringField(TEXT("windowEndedAt"), ActiveBatch.WindowEndedAt.ToIso8601());

	TArray<TSharedPtr<FJsonValue>> JsonMessages;
	for (const FTempleTalkBatchMessage& Message : ActiveBatch.Messages)
	{
		TSharedRef<FJsonObject> JsonMessage = MakeShared<FJsonObject>();
		JsonMessage->SetStringField(TEXT("uid"), Message.Uid);
		JsonMessage->SetStringField(TEXT("userId"), Message.UserId);
		JsonMessage->SetStringField(TEXT("username"), Message.Username);
		JsonMessage->SetStringField(TEXT("message"), Message.Message);
		JsonMessages.Add(MakeShared<FJsonValueObject>(JsonMessage));
	}
	Root->SetArrayField(TEXT("messages"), JsonMessages);

	FString Body;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Root, Writer);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UTempleAIBatchClient::OnHttpResponse);
	Request->SetURL(EndpointUrl);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	if (!ClientToken.IsEmpty())
	{
		Request->SetHeader(TEXT("X-Temple-Client-Token"), ClientToken);
	}
	Request->SetContentAsString(Body);
	Request->SetTimeout(RequestTimeoutSeconds);

	ActiveRequest = Request;
	Request->ProcessRequest();
}

void UTempleAIBatchClient::OnHttpResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (Request != ActiveRequest)
	{
		return; //취소되었거나 이전 배치의 응답
	}

	const int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
	const bool bRetryable = !bWasSuccessful || !Response.IsValid() || ResponseCode >= 500;
	if (bRetryable && ActiveBatch.RetryCount < MaxRetryCount)
	{
		++ActiveBatch.RetryCount;
		SendActiveRequest();
		return;
	}

	const FString BatchId = ActiveBatch.BatchId;

	if (!bWasSuccessful || !Response.IsValid() || ResponseCode < 200 || ResponseCode >= 300)
	{
		const FString Error = FString::Printf(TEXT("Temple AI 배치 요청 실패: batchId=%s, HTTP=%d"), *BatchId, ResponseCode);
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		Finish();
		OnBatchFailed.Broadcast(BatchId, Error);
		return;
	}

	FTempleTalkBatchResult Result;
	FString ParseError;
	if (!TryParseResult(Response->GetContentAsString(), Result, ParseError))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *ParseError);
		Finish();
		OnBatchFailed.Broadcast(BatchId, ParseError);
		return;
	}

	for (const FTempleIndividualResponse& Individual : Result.Responses)
	{
		UE_LOG(LogTemp, Log, TEXT("Temple AI 개별 응답 [%s] priority=%d: %s"), *Individual.Uid, Individual.Priority, *Individual.ResponseText);
	}

	Finish();
	OnBatchSucceeded.Broadcast(Result);
}

bool UTempleAIBatchClient::TryParseResult(const FString& Body, FTempleTalkBatchResult& OutResult, FString& OutError) const
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid()
		|| !JsonObject->HasTypedField<EJson::Number>(TEXT("schemaVersion"))
		|| !JsonObject->HasTypedField<EJson::String>(TEXT("batchId"))
		|| !JsonObject->HasTypedField<EJson::Array>(TEXT("responses"))
		|| !JsonObject->HasTypedField<EJson::Number>(TEXT("acceptedCount"))
		|| !JsonObject->HasTypedField<EJson::Number>(TEXT("rejectedCount")))
	{
		OutError = TEXT("Temple AI 응답 JSON을 읽을 수 없습니다.");
		return false;
	}

	OutResult.SchemaVersion = static_cast<int32>(JsonObject->GetIntegerField(TEXT("schemaVersion")));
	OutResult.BatchId = JsonObject->GetStringField(TEXT("batchId"));
	OutResult.AcceptedCount = static_cast<int32>(JsonObject->GetIntegerField(TEXT("acceptedCount")));
	OutResult.RejectedCount = static_cast<int32>(JsonObject->GetIntegerField(TEXT("rejectedCount")));

	for (const TSharedPtr<FJsonValue>& Value : JsonObject->GetArrayField(TEXT("responses")))
	{
		const TSharedPtr<FJsonObject> Item = Value->AsObject();
		if (!Item.IsValid()
			|| !Item->HasTypedField<EJson::String>(TEXT("uid"))
			|| !Item->HasTypedField<EJson::String>(TEXT("username"))
			|| !Item->HasTypedField<EJson::Number>(TEXT("priority"))
			|| !Item->HasTypedField<EJson::String>(TEXT("responseText"))
			|| !Item->HasTypedField<EJson::String>(TEXT("emotion")))
		{
			continue;
		}

		FTempleIndividualResponse Parsed;
		Parsed.Uid = Item->GetStringField(TEXT("uid"));
		Parsed.Username = Item->GetStringField(TEXT("username"));
		Parsed.Priority = static_cast<int32>(Item->GetIntegerField(TEXT("priority")));
		Parsed.ResponseText = Item->GetStringField(TEXT("responseText"));
		Parsed.Emotion = Item->GetStringField(TEXT("emotion"));
		if (!Parsed.Uid.IsEmpty() && !Parsed.ResponseText.IsEmpty())
		{
			OutResult.Responses.Add(MoveTemp(Parsed));
		}
	}

	if (OutResult.SchemaVersion != 1
		|| OutResult.BatchId != ActiveBatch.BatchId
		|| OutResult.AcceptedCount != OutResult.Responses.Num())
	{
		OutError = TEXT("Temple AI 응답 계약 또는 batchId가 일치하지 않습니다.");
		return false;
	}

	return true;
}

void UTempleAIBatchClient::Finish()
{
	ActiveRequest.Reset();
	ActiveBatch.RetryCount = 0;
}
