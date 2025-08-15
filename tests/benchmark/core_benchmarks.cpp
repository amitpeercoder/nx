#include <benchmark/benchmark.h>

#include "nx/core/note_id.hpp"
#include "nx/core/note.hpp"
#include "corpus_generator.hpp"

using namespace nx::core;
using namespace nx::test;

// Benchmark ULID generation
static void BM_UlidGeneration(benchmark::State& state) {
  for (auto _ : state) {
    auto id = NoteId::generate();
    benchmark::DoNotOptimize(id);
  }
}
BENCHMARK(BM_UlidGeneration);

// Benchmark ULID parsing
static void BM_UlidParsing(benchmark::State& state) {
  std::vector<std::string> ulids;
  for (int i = 0; i < 1000; ++i) {
    ulids.push_back(NoteId::generate().toString());
  }
  
  size_t index = 0;
  for (auto _ : state) {
    auto result = NoteId::fromString(ulids[index % ulids.size()]);
    benchmark::DoNotOptimize(result);
    ++index;
  }
}
BENCHMARK(BM_UlidParsing);

// Benchmark note creation
static void BM_NoteCreation(benchmark::State& state) {
  CorpusGenerator generator({.note_count = 1, .use_realistic_content = false});
  
  for (auto _ : state) {
    auto note = generator.generateNote();
    benchmark::DoNotOptimize(note);
  }
}
BENCHMARK(BM_NoteCreation);

// Benchmark realistic note creation
static void BM_RealisticNoteCreation(benchmark::State& state) {
  CorpusGenerator generator({.note_count = 1, .use_realistic_content = true});
  
  for (auto _ : state) {
    auto note = generator.generateNote();
    benchmark::DoNotOptimize(note);
  }
}
BENCHMARK(BM_RealisticNoteCreation);

// Benchmark corpus generation (various sizes)
static void BM_CorpusGeneration(benchmark::State& state) {
  int64_t note_count = state.range(0);
  
  for (auto _ : state) {
    CorpusGenerator generator({
      .note_count = static_cast<size_t>(note_count),
      .use_realistic_content = false  // Faster for benchmarking
    });
    
    auto corpus = generator.generateCorpus();
    benchmark::DoNotOptimize(corpus);
  }
  
  state.SetComplexityN(note_count);
  state.SetItemsProcessed(state.iterations() * note_count);
}
BENCHMARK(BM_CorpusGeneration)
    ->RangeMultiplier(10)
    ->Range(10, 10000)
    ->Complexity(benchmark::oN);

// Benchmark note serialization to file format
static void BM_NoteSerialization(benchmark::State& state) {
  TechnicalCorpusGenerator generator(100);
  auto notes = generator.generateCorpus();
  
  size_t index = 0;
  for (auto _ : state) {
    auto serialized = notes[index % notes.size()].toFileFormat();
    benchmark::DoNotOptimize(serialized);
    ++index;
  }
  
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_NoteSerialization);

// Benchmark note deserialization from file format
static void BM_NoteDeserialization(benchmark::State& state) {
  TechnicalCorpusGenerator generator(100);
  auto notes = generator.generateCorpus();
  
  std::vector<std::string> serialized_notes;
  for (const auto& note : notes) {
    serialized_notes.push_back(note.toFileFormat());
  }
  
  size_t index = 0;
  for (auto _ : state) {
    auto result = Note::fromFileFormat(serialized_notes[index % serialized_notes.size()]);
    benchmark::DoNotOptimize(result);
    ++index;
  }
  
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_NoteDeserialization);

// Memory usage benchmark for large corpus
static void BM_CorpusMemoryUsage(benchmark::State& state) {
  int64_t note_count = state.range(0);
  
  for (auto _ : state) {
    state.PauseTiming();
    CorpusGenerator generator({
      .note_count = static_cast<size_t>(note_count),
      .use_realistic_content = true
    });
    state.ResumeTiming();
    
    auto corpus = generator.generateCorpus();
    
    // Calculate approximate memory usage
    size_t total_memory = 0;
    for (const auto& note : corpus) {
      total_memory += note.title().size();
      total_memory += note.content().size();
      total_memory += note.tags().size() * 20;  // Approximate tag size
      total_memory += sizeof(Note);
    }
    
    state.counters["MemoryPerNote"] = static_cast<double>(total_memory) / note_count;
    state.counters["TotalMemoryMB"] = static_cast<double>(total_memory) / (1024 * 1024);
    
    benchmark::DoNotOptimize(corpus);
  }
  
  state.SetComplexityN(note_count);
}
BENCHMARK(BM_CorpusMemoryUsage)
    ->RangeMultiplier(10)
    ->Range(100, 100000)
    ->Complexity(benchmark::oN);

// Benchmark different corpus types
static void BM_TechnicalCorpus(benchmark::State& state) {
  for (auto _ : state) {
    TechnicalCorpusGenerator generator(1000);
    auto corpus = generator.generateCorpus();
    benchmark::DoNotOptimize(corpus);
  }
}
BENCHMARK(BM_TechnicalCorpus);

static void BM_PersonalCorpus(benchmark::State& state) {
  for (auto _ : state) {
    PersonalCorpusGenerator generator(1000);
    auto corpus = generator.generateCorpus();
    benchmark::DoNotOptimize(corpus);
  }
}
BENCHMARK(BM_PersonalCorpus);

static void BM_MeetingNotesCorpus(benchmark::State& state) {
  for (auto _ : state) {
    MeetingNotesGenerator generator(1000);
    auto corpus = generator.generateCorpus();
    benchmark::DoNotOptimize(corpus);
  }
}
BENCHMARK(BM_MeetingNotesCorpus);

BENCHMARK_MAIN();