// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PROTOBUF_QT_GENERATEDMESSAGEREFLECTION_H_
#define PROTOBUF_QT_GENERATEDMESSAGEREFLECTION_H_

#define private public  // ahahahahha
#include <google/protobuf/generated_message_reflection.h>
#undef private

namespace protobuf_qt {

class GeneratedMessageReflection
    : public google::protobuf::internal::GeneratedMessageReflection {
 public:
  GeneratedMessageReflection(const google::protobuf::Descriptor* descriptor,
                             const google::protobuf::Message* default_instance,
                             const int offsets[],
                             int has_bits_offset,
                             int unknown_fields_offset,
                             int extensions_offset,
                             const google::protobuf::DescriptorPool* pool,
                             google::protobuf::MessageFactory* factory,
                             int object_size);

  GeneratedMessageReflection(const google::protobuf::Descriptor* descriptor,
                             const google::protobuf::Message* default_instance,
                             const int offsets[],
                             int has_bits_offset,
                             int unknown_fields_offset,
                             int extensions_offset,
                             const void* default_oneof_instance,
                             int oneof_case_offset,
                             const google::protobuf::DescriptorPool* pool,
                             google::protobuf::MessageFactory* factory,
                             int object_size);

  int FieldSize(const google::protobuf::Message& message,
                const google::protobuf::FieldDescriptor* field) const;

  std::string GetString(const google::protobuf::Message& message,
                        const google::protobuf::FieldDescriptor* field) const;
  const std::string& GetStringReference(
      const google::protobuf::Message& message,
      const google::protobuf::FieldDescriptor* field,
      std::string* scratch) const;
  std::string GetRepeatedString(
      const google::protobuf::Message& message,
      const google::protobuf::FieldDescriptor* field,
      int index) const;
  const std::string& GetRepeatedStringReference(
      const google::protobuf::Message& message,
      const google::protobuf::FieldDescriptor* field,
      int index,
      std::string* scratch) const;

  int32_t  GetRepeatedInt32 (const google::protobuf::Message& message,
                             const google::protobuf::FieldDescriptor* field, int index) const;
  int64_t  GetRepeatedInt64 (const google::protobuf::Message& message,
                             const google::protobuf::FieldDescriptor* field, int index) const;
  uint32_t GetRepeatedUInt32(const google::protobuf::Message& message,
                             const google::protobuf::FieldDescriptor* field, int index) const;
  uint64_t GetRepeatedUInt64(const google::protobuf::Message& message,
                             const google::protobuf::FieldDescriptor* field, int index) const;
  float    GetRepeatedFloat (const google::protobuf::Message& message,
                             const google::protobuf::FieldDescriptor* field, int index) const;
  double   GetRepeatedDouble(const google::protobuf::Message& message,
                             const google::protobuf::FieldDescriptor* field, int index) const;
  bool     GetRepeatedBool  (const google::protobuf::Message& message,
                             const google::protobuf::FieldDescriptor* field, int index) const;

 private:
  template <typename Type>
  inline const Type& GetRaw(
      const google::protobuf::Message& message,
      const google::protobuf::FieldDescriptor* field) const;
  template <typename Type>
  inline const Type& DefaultRaw(
      const google::protobuf::FieldDescriptor* field) const;
  inline uint32_t GetOneofCase(
      const google::protobuf::Message& message,
      const google::protobuf::OneofDescriptor* oneof_descriptor) const;

  inline bool HasOneofField(
      const google::protobuf::Message& message,
      const google::protobuf::FieldDescriptor* field) const;

  template <typename Type>
  inline const Type& GetField(
      const google::protobuf::Message& message,
      const google::protobuf::FieldDescriptor* field) const;
};

}  // namespace protobuf_qt

#endif // PROTOBUF_QT_GENERATEDMESSAGEREFLECTION_H_
