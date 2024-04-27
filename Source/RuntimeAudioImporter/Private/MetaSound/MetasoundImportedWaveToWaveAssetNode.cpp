// Georgy Treshchev 2024.

#if WITH_RUNTIMEAUDIOIMPORTER_METASOUND_SUPPORT
#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "RuntimeAudioImporterDefines.h"

#include "MetasoundWave.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundVertex.h"
#include "MetaSound/MetasoundImportedWave.h"

#define LOCTEXT_NAMESPACE "MetasoundImportedWaveToWaveAsset"

namespace RuntimeAudioImporter
{
	namespace ImportedWaveToWaveAssetNodeParameterNames
	{
		METASOUND_PARAM(ParamImportedWave, "Imported Wave", "Input Imported Wave");
		METASOUND_PARAM(ParamWaveAsset, "Wave Asset", "Output Wave Asset");
	}

	class FImportedWaveToWaveAssetNodeOperator : public Metasound::TExecutableOperator<FImportedWaveToWaveAssetNodeOperator>
	{
	public:
		FImportedWaveToWaveAssetNodeOperator(const FImportedWaveReadRef& InImportedWave)
			: ImportedWave(InImportedWave)
		  , WaveAsset(Metasound::FWaveAssetWriteRef::CreateNew(nullptr))
		{
			Execute();
		}

		static const Metasound::FNodeClassMetadata& GetNodeInfo()
		{
			auto InitNodeInfo = []() -> Metasound::FNodeClassMetadata
			{
				Metasound::FNodeClassMetadata Metadata
				{
					Metasound::FNodeClassName{"RuntimeAudioImporter", "ImportedWaveToWaveAsset", ""},
					1, // Major Version
					0, // Minor Version
					LOCTEXT("ImportedWaveToWaveAssetNode_Name", "Imported Wave To Wave Asset"),
					LOCTEXT("ImportedWaveToWaveAssetNode_Description", "Converts Imported Wave to Wave Asset parameter."),
					RuntimeAudioImporter::PluginAuthor,
					RuntimeAudioImporter::PluginNodeMissingPrompt,
					GetDefaultInterface(),
					{METASOUND_LOCTEXT("RuntimeAudioImporter_Metasound_Category", "RuntimeAudioImporter")},
					{
						METASOUND_LOCTEXT("RuntimeAudioImporter_Metasound_Keyword", "RuntimeAudioImported"),
						METASOUND_LOCTEXT("ImportedSoundWave_Metasound_Keyword", "ImportedSoundWave")
					},
					{}
				};

				return Metadata;
			};

			static const Metasound::FNodeClassMetadata Info = InitNodeInfo();

			return Info;
		}

		static const Metasound::FVertexInterface& GetDefaultInterface()
		{
			using namespace Metasound;
			using namespace ImportedWaveToWaveAssetNodeParameterNames;

			static const FVertexInterface DefaultInterface(
				FInputVertexInterface(TInputDataVertex<FImportedWave>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamImportedWave))
				),
				FOutputVertexInterface(TOutputDataVertex<FWaveAsset>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamWaveAsset))
				)
			);

			return DefaultInterface;
		}

#if UE_VERSION_OLDER_THAN(5, 4, 0)
		static TUniquePtr<IOperator> CreateOperator(const Metasound::FCreateOperatorParams& InParams, Metasound::FBuildErrorArray& OutErrors)
		{
			using namespace Metasound;
			using namespace ImportedWaveToWaveAssetNodeParameterNames;

			const FDataReferenceCollection& InputDataRefs = InParams.InputDataReferences;

			FImportedWaveReadRef ImportedWaveIn = InputDataRefs.GetDataReadReferenceOrConstruct<FImportedWave>(METASOUND_GET_PARAM_NAME(ParamImportedWave));

			return MakeUnique<FImportedWaveToWaveAssetNodeOperator>(ImportedWaveIn);
		}
#else
		static TUniquePtr<IOperator> CreateOperator(const Metasound::FBuildOperatorParams& InParams, Metasound::FBuildResults& OutResults)
		{
			using namespace Metasound;
			using namespace ImportedWaveToWaveAssetNodeParameterNames;

			const FInputVertexInterfaceData& InputDataRefs = InParams.InputData;

			FImportedWaveReadRef ImportedWaveIn = InputDataRefs.GetOrCreateDefaultDataReadReference<FImportedWave>(METASOUND_GET_PARAM_NAME(ParamImportedWave), InParams.OperatorSettings);

			return MakeUnique<FImportedWaveToWaveAssetNodeOperator>(ImportedWaveIn);
		}
#endif

		virtual Metasound::FDataReferenceCollection GetInputs() const override
		{
			using namespace Metasound;
			using namespace ImportedWaveToWaveAssetNodeParameterNames;

			FDataReferenceCollection InputDataReferences;
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(ParamImportedWave), FImportedWaveReadRef(ImportedWave));

			return InputDataReferences;
		}

		virtual Metasound::FDataReferenceCollection GetOutputs() const override
		{
			using namespace Metasound;
			using namespace ImportedWaveToWaveAssetNodeParameterNames;

			FDataReferenceCollection OutputDataReferences;
			OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(ParamWaveAsset), FWaveAssetWriteRef(WaveAsset));

			return OutputDataReferences;
		}

		void Execute()
		{
			using namespace Metasound;
			if (ImportedWave->IsSoundWaveValid())
			{
				if (!WaveAsset->GetSoundWaveProxy().IsValid())
				{
					UE_LOG(LogRuntimeAudioImporter, Log, TEXT("Successfully transmitted proxy data from Imported Wave to Wave Asset"));
					*WaveAsset = FWaveAsset(MakeShared<FSoundWaveProxy>(*ImportedWave->GetSoundWaveProxy().Get()));
				}
			}
		}

	private:
		FImportedWaveReadRef ImportedWave;
		Metasound::FWaveAssetWriteRef WaveAsset;
	};

	class FImportedWaveToWaveAssetNode : public Metasound::FNodeFacade
	{
	public:
		FImportedWaveToWaveAssetNode(const Metasound::FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, Metasound::TFacadeOperatorClass<FImportedWaveToWaveAssetNodeOperator>())
		{
		}
	};

	METASOUND_REGISTER_NODE(FImportedWaveToWaveAssetNode);
}

#undef LOCTEXT_NAMESPACE
#endif
