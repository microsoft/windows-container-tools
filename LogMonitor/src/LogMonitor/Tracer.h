//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

#include <string>

#include "opentelemetry/sdk/version/version.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"


namespace trace = opentelemetry::trace;
namespace nostd = opentelemetry::nostd;

namespace trace_sdk = opentelemetry::sdk::trace;
namespace trace_exporter = opentelemetry::exporter::trace;

namespace
{
    nostd::shared_ptr<trace::Tracer> get_tracer(std::string tracer_name) {
        auto provider = trace::Provider::GetTracerProvider();
        return provider->GetTracer(tracer_name, OPENTELEMETRY_SDK_VERSION);
    }

    void InitTracer() {
        auto exporter = trace_exporter::OStreamSpanExporterFactory::Create();
        auto processor = trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));

        std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
            trace_sdk::TracerProviderFactory::Create(std::move(processor));

        // Set the global trace provider
        trace::Provider::SetTracerProvider(provider);
    }

    void CleanupTracer() {
        std::shared_ptr<opentelemetry::trace::TracerProvider> none;
        trace_api::Provider::SetTracerProvider(none);
    }

    void ReportSystemInfoTelemetry(SystemInfo& systemInfo) {
        int CurrentMinorVersionNumber = systemInfo.GetRegCurVersion().CurrentMinorVersionNumber;
        int CurrentMajorVersionNumber = systemInfo.GetRegCurVersion().CurrentMajorVersionNumber;

        auto span = get_tracer("SYSTEM_INFO")
        ->StartSpan("SYS_INFO_SPAN", {
            {
                Utility::W_STR_TO_STR(BUILD_LAB),
                Utility::W_STR_TO_STR(systemInfo.GetRegCurVersion().BuildLab)
            },
            {
                Utility::W_STR_TO_STR(BUILD_LAB_EX),
                Utility::W_STR_TO_STR(systemInfo.GetRegCurVersion().BuildLabEx)
            },
            {
                Utility::W_STR_TO_STR(CURRENT_BUILD_NUMBER),
                Utility::W_STR_TO_STR(systemInfo.GetRegCurVersion().CurrentBuildNumber)
            },
            {
                Utility::W_STR_TO_STR(INSTALLATION_TYPE),
                Utility::W_STR_TO_STR(systemInfo.GetRegCurVersion().InstallationType)
            },
            {
                Utility::W_STR_TO_STR(DISPLAY_VERSION),
                Utility::W_STR_TO_STR(systemInfo.GetRegCurVersion().DisplayVersion)
            },
             {
                 Utility::W_STR_TO_STR(CURR_MINOR_VER_NUM),
                 CurrentMinorVersionNumber
             },
             {
                 Utility::W_STR_TO_STR(CUR_MAJOR_VER_NUM),
                 CurrentMajorVersionNumber
             },
            {
                Utility::W_STR_TO_STR(PRODUCT_NAME),
                Utility::W_STR_TO_STR(systemInfo.GetRegCurVersion().ProductName)
            }
        });

    span->End();
    }
}  // namespace

