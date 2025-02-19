// Copyright 2019 The MediaPipe Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MEDIAPIPE_FRAMEWORK_CALCULATOR_CONTEXT_H_
#define MEDIAPIPE_FRAMEWORK_CALCULATOR_CONTEXT_H_

#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "mediapipe/framework/calculator_state.h"
#include "mediapipe/framework/counter.h"
#include "mediapipe/framework/graph_service.h"
#include "mediapipe/framework/input_stream_shard.h"
#include "mediapipe/framework/output_stream_shard.h"
#include "mediapipe/framework/packet_set.h"
#include "mediapipe/framework/port.h"
#include "mediapipe/framework/port/any_proto.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/framework/timestamp.h"

namespace mediapipe {

// A CalculatorContext provides information about the graph it is running
// inside of through a number of accessor functions: Inputs(), Outputs(),
// InputSidePackets(), Options(), etc.
//
// CalculatorBase APIs, such as CalculatorBase::Open(CalculatorContext* cc),
// CalculatorBase::Process(CalculatorContext* cc), and
// CalculatorBase::Close(CalculatorContext* cc), will only interact with
// its own CalculatorContext object for exchanging data with the framework.
class CalculatorContext {
 public:
  CalculatorContext(CalculatorState* calculator_state,
                    std::shared_ptr<tool::TagMap> input_tag_map,
                    std::shared_ptr<tool::TagMap> output_tag_map)
      : calculator_state_(calculator_state),
        inputs_(std::move(input_tag_map)),
        outputs_(std::move(output_tag_map)) {}

  CalculatorContext(const CalculatorContext&) = delete;
  CalculatorContext& operator=(const CalculatorContext&) = delete;

  const std::string& NodeName() const;
  int NodeId() const;
  const std::string& CalculatorType() const;
  // Returns the options given to this calculator. The Calculator or
  // CalculatorBase implementation may get its options by calling
  // GetExtension() on the result.
  const CalculatorOptions& Options() const;

  // Returns the options given to this calculator.  Template argument T must
  // be the type of the protobuf extension message or the protobuf::Any
  // message containing the options.
  template <class T>
  const T& Options() const {
    return calculator_state_->Options<T>();
  }

  // Returns a counter using the graph's counter factory. The counter's name is
  // the passed-in name, prefixed by the calculator node's name (if present) or
  // the calculator's type (if not).
  Counter* GetCounter(const std::string& name);

  // Returns the current input timestamp, or Timestamp::Unset if there are
  // no input packets.
  Timestamp InputTimestamp() const {
    return input_timestamps_.empty() ? Timestamp::Unset()
                                     : input_timestamps_.front();
  }

  // Returns a reference to the input side packet set.
  const PacketSet& InputSidePackets() const;
  // Returns a reference to the output side packet collection.
  OutputSidePacketSet& OutputSidePackets();
  // Returns a reference to the input stream collection.
  // You may consume or move the value packets from the Inputs.
  InputStreamShardSet& Inputs();
  // Returns a const reference to the input stream collection.
  const InputStreamShardSet& Inputs() const;
  // Returns a reference to the output stream collection.
  OutputStreamShardSet& Outputs();
  // Returns a const reference to the output stream collection.
  const OutputStreamShardSet& Outputs() const;

  // Sets this packet timestamp offset for Packets going to all outputs.
  // If you only want to set the offset for a single output stream then
  // use OutputStream::SetOffset() directly.
  void SetOffset(TimestampDiff offset);

  // Returns the status of the graph run.
  //
  // NOTE: This method should only be called during CalculatorBase::Close().
  ::mediapipe::Status GraphStatus() const { return graph_status_; }

  ProfilingContext* GetProfilingContext() const {
    return calculator_state_->GetSharedProfilingContext().get();
  }

  template <typename T>
  class ServiceBinding {
   public:
    bool IsAvailable() {
      return calculator_state_->IsServiceAvailable(service_);
    }
    T& GetObject() { return calculator_state_->GetServiceObject(service_); }
    T& GetObject_() { return calculator_state_->GetServiceObject(service_); }

    ServiceBinding(CalculatorState* calculator_state,
                   const GraphService<T>& service)
        : calculator_state_(calculator_state), service_(service) {}

   private:
    CalculatorState* calculator_state_;
    const GraphService<T>& service_;
  };

  template <typename T>
  ServiceBinding<T> Service(const GraphService<T>& service) {
    return ServiceBinding<T>(calculator_state_, service);
  }

 private:
  int NumberOfTimestamps() const {
    return static_cast<int>(input_timestamps_.size());
  }

  bool HasInputTimestamp() const { return !input_timestamps_.empty(); }

  // Adds a new input timestamp by the friend class CalculatorContextManager.
  void PushInputTimestamp(Timestamp input_timestamp) {
    input_timestamps_.push(input_timestamp);
  }

  void PopInputTimestamp() {
    CHECK(!input_timestamps_.empty());
    input_timestamps_.pop();
  }

  void SetGraphStatus(const ::mediapipe::Status& status) {
    graph_status_ = status;
  }

  // Interface for the friend class Calculator.
  const InputStreamSet& InputStreams() const;
  const OutputStreamSet& OutputStreams() const;

  // Stores the shared data across all CalculatorContext objects, including
  // input side packets, calculator options, node name, etc.
  // TODO: Removes unnecessary fields from CalculatorState after
  // migrating all clients to CalculatorContext.
  CalculatorState* calculator_state_;
  InputStreamShardSet inputs_;
  OutputStreamShardSet outputs_;
  // The queue of timestamp values to Process() in this calculator context.
  std::queue<Timestamp> input_timestamps_;

  // The status of the graph run. Only used when Close() is called.
  ::mediapipe::Status graph_status_;

  // Accesses CalculatorContext for setting input timestamp.
  friend class CalculatorContextManager;
};

}  // namespace mediapipe

#endif  // MEDIAPIPE_FRAMEWORK_CALCULATOR_CONTEXT_H_
