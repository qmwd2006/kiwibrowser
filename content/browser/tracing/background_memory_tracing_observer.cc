// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_memory_tracing_observer.h"

#include "base/trace_event/heap_profiler_allocation_context_tracker.h"
#include "base/trace_event/heap_profiler_event_filter.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/trace_log.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

using base::trace_event::AllocationContextTracker;
using base::trace_event::TraceConfig;
using base::trace_event::TraceLog;

namespace content {
namespace {
const char kHeapProfilerCategoryFilter[] = "heap_profiler_category_filter";
}  // namespace

// static
BackgroundMemoryTracingObserver*
BackgroundMemoryTracingObserver::GetInstance() {
  static auto* instance = new BackgroundMemoryTracingObserver();
  return instance;
}

BackgroundMemoryTracingObserver::BackgroundMemoryTracingObserver() {}
BackgroundMemoryTracingObserver::~BackgroundMemoryTracingObserver() {}

void BackgroundMemoryTracingObserver::OnScenarioActivated(
    const BackgroundTracingConfigImpl* config) {
  if (!config) {
    DCHECK(!enabled_);
    return;
  }

  const BackgroundTracingRule* heap_profiling_rule = nullptr;
  for (const auto& rule : config->rules()) {
    if (rule->category_preset() == BackgroundTracingConfigImpl::CategoryPreset::
                                       BENCHMARK_MEMORY_LIGHT &&
        rule->args()) {
      heap_profiling_rule = rule.get();
      break;
    }
  }
  if (!heap_profiling_rule)
    return;

  enabled_ = true;

  // TODO(ssid): Add ability to enable profiling on all processes,
  // crbug.com/700245.
  AllocationContextTracker::SetCaptureMode(
      AllocationContextTracker::CaptureMode::MIXED_STACK);

  std::string filter_string;
  if ((TraceLog::GetInstance()->enabled_modes() & TraceLog::FILTERING_MODE) ||
      !heap_profiling_rule->args()->GetString(kHeapProfilerCategoryFilter,
                                              &filter_string)) {
    return;
  }
  base::trace_event::TraceConfigCategoryFilter category_filter;
  category_filter.InitializeFromString(filter_string);
  TraceConfig::EventFilterConfig heap_profiler_filter_config(
      base::trace_event::HeapProfilerEventFilter::kName);
  heap_profiler_filter_config.SetCategoryFilter(category_filter);
  TraceConfig::EventFilters filters;
  filters.push_back(heap_profiler_filter_config);
  TraceConfig filtering_trace_config;
  filtering_trace_config.SetEventFilters(filters);
  TraceLog::GetInstance()->SetEnabled(filtering_trace_config,
                                      TraceLog::FILTERING_MODE);
}

void BackgroundMemoryTracingObserver::OnScenarioAborted() {
  if (!enabled_)
    return;
  enabled_ = false;
  base::trace_event::AllocationContextTracker::SetCaptureMode(
      AllocationContextTracker::CaptureMode::DISABLED);
  TraceLog::GetInstance()->SetDisabled(TraceLog::FILTERING_MODE);
}

void BackgroundMemoryTracingObserver::OnTracingEnabled(
    BackgroundTracingConfigImpl::CategoryPreset preset) {
  if (preset !=
      BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_MEMORY_LIGHT)
    return;

  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDumpAndAppendToTrace(
          base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED,
          base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND,
          memory_instrumentation::MemoryInstrumentation::
              RequestGlobalMemoryDumpAndAppendToTraceCallback());
}

}  // namespace content
