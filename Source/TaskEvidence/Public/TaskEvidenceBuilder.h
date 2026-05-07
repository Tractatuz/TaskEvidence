#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FJsonValue;

class TASKEVIDENCE_API FTaskEvidenceBuilder
{
public:
	FTaskEvidenceBuilder(const FString& InToolName, const FString& InOperation);

	static bool IsEnabled();
	static FString MakeRunId();

	FTaskEvidenceBuilder& SetTaskId(const FString& InTaskId);
	FTaskEvidenceBuilder& SetRunId(const FString& InRunId);
	FTaskEvidenceBuilder& SetStatus(const FString& InStatus);
	FTaskEvidenceBuilder& SetSummary(const FString& InMessage, const FString& InError = FString());

	FTaskEvidenceBuilder& AddLog(const FString& Level, const FString& Category, const FString& Message);
	FTaskEvidenceBuilder& AddFact(const FString& Key, const FString& Value, const FString& Source = FString(), double Confidence = 1.0);
	FTaskEvidenceBuilder& AddFact(const FString& Key, bool bValue, const FString& Source = FString(), double Confidence = 1.0);
	FTaskEvidenceBuilder& AddFact(const FString& Key, int32 Value, const FString& Source = FString(), double Confidence = 1.0);
	FTaskEvidenceBuilder& AddFact(const FString& Key, double Value, const FString& Source = FString(), double Confidence = 1.0);
	FTaskEvidenceBuilder& AddArtifact(const FString& FilePath, const FString& Role, const FString& Type, const FString& Description = FString());

	bool WriteToDefaultLocation(FString& OutFilePath, FString& OutError);
	bool WriteToFile(const FString& FilePath, FString& OutError);

private:
	FTaskEvidenceBuilder& AddFactValue(const FString& Key, const TSharedPtr<FJsonValue>& Value, const FString& Source, double Confidence);

	TSharedRef<FJsonObject> BuildRootObject();
	FString ResolveDefaultFilePath() const;
	static FString ResolveEvidenceRootDir();
	static FString ToIso8601(const FDateTime& Time);
	static FString SanitizePathSegment(const FString& Value);
	static FString MakeFileHash(const FString& FilePath);
	static FString MakeRelativePath(const FString& FilePath);

	FString ToolName;
	FString Operation;
	FString TaskId;
	FString RunId;
	FString Status = TEXT("running");
	FString SummaryMessage;
	FString SummaryError;
	FDateTime StartedAt;
	FDateTime EndedAt;
	TArray<TSharedPtr<FJsonValue>> Logs;
	TArray<TSharedPtr<FJsonValue>> Facts;
	TArray<TSharedPtr<FJsonValue>> Artifacts;
};
