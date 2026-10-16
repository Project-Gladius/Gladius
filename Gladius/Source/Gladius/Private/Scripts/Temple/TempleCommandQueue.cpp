#include "Scripts/Temple/TempleCommandQueue.h"
#include "Scripts/Backend/CommandDispatcher.h"

void UTempleCommandQueue::Bind(UCommandDispatcher* Dispatcher)
{
	Unbind();
	if (!Dispatcher)
	{
		return;
	}

	BoundDispatcher = Dispatcher;
	EnhanceHandle = Dispatcher->OnEnhanceCommand.AddUObject(this, &UTempleCommandQueue::AddEnhance);
	TalkHandle = Dispatcher->OnTalkCommand.AddUObject(this, &UTempleCommandQueue::AddTalk);

	if (!bWindowStarted)
	{
		ResetWindow(FDateTime::UtcNow());
	}
}

void UTempleCommandQueue::Unbind()
{
	if (UCommandDispatcher* Dispatcher = BoundDispatcher.Get())
	{
		Dispatcher->OnEnhanceCommand.Remove(EnhanceHandle);
		Dispatcher->OnTalkCommand.Remove(TalkHandle);
	}
	BoundDispatcher.Reset();
	EnhanceHandle.Reset();
	TalkHandle.Reset();
}

FTempleCycleCommands UTempleCommandQueue::TakeSnapshot()
{
	const FDateTime Now = FDateTime::UtcNow();

	FTempleCycleCommands Snapshot = MoveTemp(Current);
	Snapshot.WindowEndedAt = Now;

	ResetWindow(Now);
	return Snapshot;
}

void UTempleCommandQueue::ResetWindow(const FDateTime& WindowStartedAt)
{
	Current = FTempleCycleCommands();
	Current.EnhanceTiers.SetNum(GTempleEnhanceTierCount);
	Current.WindowStartedAt = WindowStartedAt;
	CurrentTalkUserIds.Reset();
	CurrentTalkCharacterCount = 0;
	bWindowStarted = true;
}

void UTempleCommandQueue::AddEnhance(const FString& UserId, const FString& Username, const FString& Argument)
{
	const int32 Tier = FCString::Atoi(*Argument);
	if (Tier < 1 || Tier > GTempleEnhanceTierCount)
	{
		return;
	}

	//같은 유저의 중복 강화도 모두 허용
	FParsedCommand Command;
	Command.CommandType = static_cast<ECommandType>(static_cast<uint8>(ECommandType::Enhance1) + Tier - 1);
	Command.UserId = UserId;
	Command.Username = Username;
	Command.Argument = Argument;
	Current.EnhanceTiers[Tier - 1].Commands.Add(MoveTemp(Command));
}

void UTempleCommandQueue::AddTalk(const FString& UserId, const FString& CommentId, const FString& Username, const FString& Message)
{
	const FString CleanMessage = Message.TrimStartAndEnd();
	if (CleanMessage.IsEmpty() || CleanMessage.Len() > MaxTalkMessageLength)
	{
		return;
	}
	if (ProcessedTalkUids.Contains(CommentId) || CurrentTalkUserIds.Contains(UserId))
	{
		return;
	}

	ProcessedTalkUids.Add(CommentId);
	if (Current.Talks.Num() >= MaxTalksPerCycle
		|| CurrentTalkCharacterCount + CleanMessage.Len() > MaxTotalTalkCharacters)
	{
		UE_LOG(LogTemp, Warning, TEXT("Temple AI 배치 제한으로 대화를 제외했습니다: %s"), *CommentId);
		return;
	}

	FTempleTalkBatchMessage Talk;
	Talk.Uid = CommentId;
	Talk.UserId = UserId;
	Talk.Username = Username.IsEmpty() ? UserId : Username;
	Talk.Message = CleanMessage;
	Current.Talks.Add(MoveTemp(Talk));
	CurrentTalkUserIds.Add(UserId);
	CurrentTalkCharacterCount += CleanMessage.Len();
}
