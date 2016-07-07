/*
 * Copyright 2016 Fixstars Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include "m3bp/configuration.hpp"
#include "m3bp/flow_graph.hpp"
#include "graph/logical_graph_builder.hpp"
#include "util/execution_util.hpp"

namespace {

static std::atomic<bool> g_successor_finalized(false);

class LazyFinalizableTask : public m3bp::ProcessorBase {
public:
	LazyFinalizableTask()
		: m3bp::ProcessorBase(
			{ },
			{ m3bp::OutputPort("out") })
	{
		task_count(1);
		lazy_finalizable(true);
	}

	virtual void global_finalize(m3bp::Task &) override {
		namespace chrono = std::chrono;
		const auto time_limit =
			chrono::steady_clock::now() + chrono::seconds(2);
		while(!g_successor_finalized.load()){
			if(chrono::steady_clock::now() > time_limit){
				FAIL();
				break;
			}
		}
		g_successor_finalized.store(false);
	}
	virtual void run(m3bp::Task &) override { }
};

class SuccessorTask : public m3bp::ProcessorBase {
public:
	SuccessorTask()
		: m3bp::ProcessorBase(
			{
				m3bp::InputPort("in")
					.movement(m3bp::Movement::ONE_TO_ONE)
			},
			{ })
	{ }

	virtual void global_finalize(m3bp::Task &) override {
		g_successor_finalized.store(true);
	}
	virtual void run(m3bp::Task &) override { }
};

}

TEST(LogicalGraph, LazyFinalization){
	const auto config = m3bp::Configuration()
		.max_concurrency(4);

	m3bp::FlowGraph fgraph;
	auto entry = fgraph.add_vertex("entry", LazyFinalizableTask());
	auto terminal = fgraph.add_vertex("terminal", SuccessorTask());
	fgraph.add_edge(entry.output_port(0), terminal.input_port(0));

	auto lgraph = m3bp::build_logical_graph(fgraph, config);
	util::execute_logical_graph(lgraph, config.max_concurrency());
}
