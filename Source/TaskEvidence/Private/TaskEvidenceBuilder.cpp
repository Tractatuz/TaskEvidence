#include "TaskEvidenceBuilder.h"

#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProperties.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FTaskEvidenceBuilder::FTaskEvidenceBuilder(const FString& InToolName, const FString& InOperation)
	: ToolName(InToolName)
	, Operation(InOperation)
	, RunId(MakeRunId())
	, StartedAt(FDateTime::UtcNow())
{
	FString CommandLineTaskId;
	if (FParse::Value(FCommandLine::Get(), TEXT("TaskEvidenceTaskId="), CommandLineTaskId))
	{
		TaskId = CommandLineTaskId;
	}

	FString CommandLineRunId;
	if (FParse::Value(FCommandLine::Get(), TEXT("TaskEvidenceRunId="), CommandLineRunId) && !CommandLineRunId.IsEmpty())
	{
		RunId = CommandLineRunId;
	}
}

bool FTaskEvidenceBuilder::IsEnabled()
{
	return !FParse::Param(FCommandLine::Get(), TEXT("TaskEvidenceDisabled"));
}

FString FTaskEvidenceBuilder::MakeRunId()
{
	const FString Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"));
	const FString Guid = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
	return FString::Printf(TEXT("%s-%s"), *Timestamp, *Guid);
}

FTaskEvidenceBuilder& FTaskEvidenceBuilder::SetTaskId(const FString& InTaskId)
{
	TaskId = InTaskId;
	return *this;
}

FTaskEvidenceBuilder& FTaskEvidenceBuilder::SetRunId(const FString& InRunId)
{
	if (!InRunId.IsEmpty())
	{
		RunId = InRunId;
	}
	return *this;
}

FTaskEvidenceBuilder& FTaskEvidenceBuilder::SetStatus(const FString& InStatus)
{
	Status = InStatus;
	return *this;
}

FTaskEvidenceBuilder& FTaskEvidenceBuilder::SetSummary(const FString& InMessage, const FString& InError)
{
	SummaryMessage = InMessage;
	SummaryError = InError;
	return *this;
}

FTaskEvidenceBuilder& FTaskEvidenceBuilder::AddLog(const FString& Level, const FString& Category, const FString& Message)
{
	TSharedPtr<FJsonObject> Log = MakeShared<FJsonObject>();
	Log->SetStringField(TEXT("time"), ToIso8601(FDateTime::UtcNow()));
	Log->SetStringField(TEXT("level"), Level);
	Log->SetStringField(TEXT("category"), Category);
	Log->SetStringField(TEXT("message"), Message);
	Logs.Add(MakeShared<FJsonValueObject>(Log));
	return *this;
}

FTaskEvidenceBuilder& FTaskEvidenceBuilder::AddFact(const FString& Key, const FString& Value, const FString& Source, double Confidence)
{
	return AddFactValue(Key, MakeShared<FJsonValueString>(Value), Source, Confidence);
}

FTaskEvidenceBuilder& FTaskEvidenceBuilder::AddFact(const FString& Key, bool bValue, const FString& Source, double Confidence)
{
	return AddFactValue(Key, MakeShared<FJsonValueBoolean>(bValue), Source, Confidence);
}

FTaskEvidenceBuilder& FTaskEvidenceBuilder::AddFact(const FString& Key, int32 Value, const FString& Source, double Confidence)
{
	return AddFactValue(Key, MakeShared<FJsonValueNumber>(Value), Source, Confidence);
}

FTaskEvidenceBuilder& FTaskEvidenceBuilder::AddFact(const FString& Key, double Value, const FString& Source, double Confidence)
{
	return AddFactValue(Key, MakeShared<FJsonValueNumber>(Value), Source, Confidence);
}

FTaskEvidenceBuilder& FTaskEvidenceBuilder::AddArtifact(const FString& FilePath, const FString& Role, const FString& Type, const FString& Description)
{
	if (FilePath.IsEmpty())
	{
		return *this;
	}

	TSharedPtr<FJsonObject> Artifact = MakeShared<FJsonObject>();
	Artifact->SetStringField(TEXT("path"), MakeRelativePath(FilePath));
	Artifact->SetStringField(TEXT("role"), Role);
	Artifact->SetStringField(TEXT("type"), Type);
	Artifact->SetStringField(TEXT("description"), Description);

	const int64 FileSize = IFileManager::Get().FileSize(*FilePath);
	if (FileSize >= 0)
	{
		Artifact->SetNumberField(TEXT("size_bytes"), static_cast<double>(FileSize));
		Artifact->SetStringField(TEXT("content_hash"), MakeFileHash(FilePath));
	}

	Artifacts.Add(MakeShared<FJsonValueObject>(Artifact));
	return *this;
}

bool FTaskEvidenceBuilder::WriteToDefaultLocation(FString& OutFilePath, FString& OutError)
{
	OutFilePath = ResolveDefaultFilePath();
	return WriteToFile(OutFilePath, OutError);
}

bool FTaskEvidenceBuilder::WriteToFile(const FString& FilePath, FString& OutError)
{
	if (!IsEnabled())
	{
		OutError.Reset();
		return false;
	}

	if (FilePath.IsEmpty())
	{
		OutError = TEXT("TaskEvidence output path is empty.");
		return false;
	}

	EndedAt = FDateTime::UtcNow();

	FString JsonText;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(BuildRootObject(), Writer))
	{
		OutError = TEXT("Could not serialize task evidence JSON.");
		return false;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);
	if (!FFileHelper::SaveStringToFile(JsonText, *FilePath))
	{
		OutError = FString::Printf(TEXT("Could not write task evidence file: %s"), *FilePath);
		return false;
	}

	return true;
}

FTaskEvidenceBuilder& FTaskEvidenceBuilder::AddFactValue(const FString& Key, const TSharedPtr<FJsonValue>& Value, const FString& Source, double Confidence)
{
	TSharedPtr<FJsonObject> Fact = MakeShared<FJsonObject>();
	Fact->SetStringField(TEXT("key"), Key);
	Fact->SetField(TEXT("value"), Value);
	Fact->SetStringField(TEXT("source"), Source.IsEmpty() ? ToolName : Source);
	Fact->SetNumberField(TEXT("confidence"), Confidence);
	Facts.Add(MakeShared<FJsonValueObject>(Fact));
	return *this;
}

TSharedRef<FJsonObject> FTaskEvidenceBuilder::BuildRootObject()
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("ue.task_evidence.v1"));

	TSharedPtr<FJsonObject> Task = MakeShared<FJsonObject>();
	Task->SetStringField(TEXT("id"), TaskId);
	Task->SetStringField(TEXT("run_id"), RunId);
	Task->SetStringField(TEXT("tool"), ToolName);
	Task->SetStringField(TEXT("operation"), Operation);
	Task->SetStringField(TEXT("status"), Status);
	Task->SetStringField(TEXT("started_at"), ToIso8601(StartedAt));
	Task->SetStringField(TEXT("ended_at"), ToIso8601(EndedAt));
	Task->SetNumberField(TEXT("duration_seconds"), (EndedAt - StartedAt).GetTotalSeconds());
	Root->SetObjectField(TEXT("task"), Task);

	TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetStringField(TEXT("message"), SummaryMessage);
	Summary->SetStringField(TEXT("error"), SummaryError);
	Root->SetObjectField(TEXT("summary"), Summary);

	Root->SetArrayField(TEXT("facts"), Facts);
	Root->SetArrayField(TEXT("logs"), Logs);
	Root->SetArrayField(TEXT("artifacts"), Artifacts);

	TSharedPtr<FJsonObject> Environment = MakeShared<FJsonObject>();
	Environment->SetStringField(TEXT("project"), FApp::GetProjectName());
	Environment->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
	Environment->SetStringField(TEXT("platform"), FPlatformProperties::PlatformName());
	Root->SetObjectField(TEXT("environment"), Environment);

	return Root;
}

FString FTaskEvidenceBuilder::ResolveDefaultFilePath() const
{
	return ResolveEvidenceRootDir() / SanitizePathSegment(ToolName) / SanitizePathSegment(RunId) / TEXT("evidence.json");
}

FString FTaskEvidenceBuilder::ResolveEvidenceRootDir()
{
	FString Directory;
	if (FParse::Value(FCommandLine::Get(), TEXT("TaskEvidenceDir="), Directory) && !Directory.IsEmpty())
	{
		return FPaths::ConvertRelativePathToFull(Directory);
	}

	return FPaths::ProjectSavedDir() / TEXT("TaskEvidence");
}

FString FTaskEvidenceBuilder::ToIso8601(const FDateTime& Time)
{
	return Time.ToIso8601();
}

FString FTaskEvidenceBuilder::SanitizePathSegment(const FString& Value)
{
	FString Sanitized = Value;
	const TCHAR* InvalidChars = TEXT("<>:\"/\\|?*");
	for (int32 CharIndex = 0; InvalidChars[CharIndex] != TEXT('\0'); ++CharIndex)
	{
		Sanitized.ReplaceCharInline(InvalidChars[CharIndex], TEXT('_'));
	}
	return Sanitized.IsEmpty() ? TEXT("unnamed") : Sanitized;
}

FString FTaskEvidenceBuilder::MakeFileHash(const FString& FilePath)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *FilePath))
	{
		return FString();
	}

	uint32 Hash = 2166136261u;
	for (uint8 Byte : Bytes)
	{
		Hash ^= Byte;
		Hash *= 16777619u;
	}

	return FString::Printf(TEXT("fnv1a32:%08x"), Hash);
}

FString FTaskEvidenceBuilder::MakeRelativePath(const FString& FilePath)
{
	FString RelativePath = FPaths::ConvertRelativePathToFull(FilePath);
	FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::MakePathRelativeTo(RelativePath, *ProjectDir);
	return RelativePath;
}
