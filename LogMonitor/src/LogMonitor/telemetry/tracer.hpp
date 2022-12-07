#define NOMINMAX

#include "opentelemetry/sdk/version/version.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/sdk/trace/simple_processor.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"

namespace trace = opentelemetry::trace;
namespace nostd = opentelemetry::nostd;
namespace sdktrace = opentelemetry::sdk::trace;
namespace traceexporter = opentelemetry::exporter::trace;

using namespace std;


void initTracer()
{
    auto exporter = traceexporter::OStreamSpanExporterFactory::Create();

    auto processor = sdktrace::SimpleSpanProcessorFactory::Create(std::move(exporter));

    std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
        sdktrace::TracerProviderFactory::Create(std::move(processor));

    trace::Provider::SetTracerProvider(provider);
}


nostd::shared_ptr<trace::Tracer> getTracer(string serviceName)
{
    auto provider = opentelemetry::trace::Provider::GetTracerProvider();
    return provider->GetTracer(serviceName, OPENTELEMETRY_SDK_VERSION);
}

void tracer(string serviceName, string operationName)
{
    // auto span = getTracer(serviceName)->StartSpan(operationName);

    // span->SetAttribute("service.name", serviceName);

    

    // auto scope = getTracer(serviceName)->WithActiveSpan(span);

    // do something with span, and scope then...
    //
    // span->End();

    auto scoped_span = trace::Scope(getTracer(serviceName)->StartSpan(operationName));
}