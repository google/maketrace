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

#include "protobuf_qt/generated_message_reflection.h"

#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/extension_set.h>

#include <QByteArray>
#include <QString>
#include <QStringList>

using google::protobuf::Descriptor;
using google::protobuf::DescriptorPool;
using google::protobuf::FieldDescriptor;
using google::protobuf::FieldOptions;
using google::protobuf::Message;
using google::protobuf::MessageFactory;
using google::protobuf::OneofDescriptor;
using google::protobuf::internal::ExtensionSet;
using google::protobuf::internal::GetEmptyString;

using std::string;

namespace protobuf_qt {

template <typename Type>
inline const Type& GeneratedMessageReflection::GetRaw(
    const Message& message, const FieldDescriptor* field) const {
  if (field->containing_oneof() && !HasOneofField(message, field)) {
    return DefaultRaw<Type>(field);
  }
  int index = field->containing_oneof() ?
      descriptor_->field_count() + field->containing_oneof()->index() :
      field->index();
  const void* ptr = reinterpret_cast<const uint8_t*>(&message) +
      offsets_[index];
  return *reinterpret_cast<const Type*>(ptr);
}

template <typename Type>
inline const Type& GeneratedMessageReflection::GetField(
    const Message& message, const FieldDescriptor* field) const {
  return GetRaw<Type>(message, field);
}

inline bool GeneratedMessageReflection::HasOneofField(
    const Message& message, const FieldDescriptor* field) const {
  return (GetOneofCase(message, field->containing_oneof()) == field->number());
}

template <typename Type>
inline const Type& GeneratedMessageReflection::DefaultRaw(
    const FieldDescriptor* field) const {
  const void* ptr = field->containing_oneof() ?
      reinterpret_cast<const uint8_t*>(default_oneof_instance_) +
      offsets_[field->index()] :
      reinterpret_cast<const uint8_t*>(default_instance_) +
      offsets_[field->index()];
  return *reinterpret_cast<const Type*>(ptr);
}

inline uint32_t GeneratedMessageReflection::GetOneofCase(
    const Message& message,
    const OneofDescriptor* oneof_descriptor) const {
  const void* ptr = reinterpret_cast<const uint8_t*>(&message)
      + oneof_case_offset_;
  return reinterpret_cast<const uint32_t*>(ptr)[oneof_descriptor->index()];
}



GeneratedMessageReflection::GeneratedMessageReflection(
    const Descriptor* descriptor,
    const Message* default_instance,
    const int offsets[],
    int has_bits_offset,
    int unknown_fields_offset,
    int extensions_offset,
    const DescriptorPool* pool,
    MessageFactory* factory,
    int object_size)
    : google::protobuf::internal::GeneratedMessageReflection(
          descriptor,
          default_instance,
          offsets,
          has_bits_offset,
          unknown_fields_offset,
          extensions_offset,
          pool,
          factory,
          object_size) {
}

GeneratedMessageReflection::GeneratedMessageReflection(
    const Descriptor* descriptor,
    const Message* default_instance,
    const int offsets[],
    int has_bits_offset,
    int unknown_fields_offset,
    int extensions_offset,
    const void* default_oneof_instance,
    int oneof_case_offset,
    const DescriptorPool* pool,
    MessageFactory* factory,
    int object_size)
    : google::protobuf::internal::GeneratedMessageReflection(
          descriptor,
          default_instance,
          offsets,
          has_bits_offset,
          unknown_fields_offset,
          extensions_offset,
          default_oneof_instance,
          oneof_case_offset,
          pool,
          factory,
          object_size) {
}

int GeneratedMessageReflection::FieldSize(const Message& message,
                                          const FieldDescriptor* field) const {
#define GET_FIELD_SIZE(TYPE, CPP_TYPE) \
  case FieldDescriptor::CPPTYPE_##CPP_TYPE: \
    return GetRaw<QList<TYPE>>(message, field).count()

  if (!field->is_extension()) {
    switch (field->cpp_type()) {
      GET_FIELD_SIZE(QString,    STRING);
      GET_FIELD_SIZE(int32_t,    INT32);
      GET_FIELD_SIZE(int64_t,    INT64);
      GET_FIELD_SIZE(uint32_t,   UINT32);
      GET_FIELD_SIZE(uint64_t,   UINT64);
      GET_FIELD_SIZE(float,      FLOAT);
      GET_FIELD_SIZE(double,     DOUBLE);
      GET_FIELD_SIZE(bool,       BOOL);
      default:
        break;
    }
  }
  return google::protobuf::internal::GeneratedMessageReflection::FieldSize(
      message, field);
}

string GeneratedMessageReflection::GetString(
    const Message& message, const FieldDescriptor* field) const {
  if (field->is_extension()) {
    return "[extensions not supported by protobuf_qt reflection]";
  }

  QByteArray bytes;
  if (field->type() == FieldDescriptor::TYPE_BYTES) {
    bytes = GetField<const QByteArray>(message, field);
  } else {
    bytes = GetField<const QString>(message, field).toUtf8();
  }
  return string(bytes.constData(), bytes.size());
}

const string& GeneratedMessageReflection::GetStringReference(
    const Message& message,
    const FieldDescriptor* field, string* scratch) const {
  *scratch = GetString(message, field);
  return *scratch;
}

string GeneratedMessageReflection::GetRepeatedString(
    const Message& message, const FieldDescriptor* field, int index) const {
  if (field->is_extension()) {
    return "[extensions not supported by protobuf_qt reflection]";
  }

  QByteArray bytes;
  if (field->type() == FieldDescriptor::TYPE_BYTES) {
    bytes = GetField<const QList<QByteArray>>(message, field)[index];
  } else {
    bytes = GetField<const QStringList>(message, field)[index].toUtf8();
  }
  return string(bytes.constData(), bytes.size());
}

const string& GeneratedMessageReflection::GetRepeatedStringReference(
    const Message& message, const FieldDescriptor* field,
    int index, string* scratch) const {
  *scratch = GetRepeatedString(message, field, index);
  return *scratch;
}

#undef DEFINE_PRIMITIVE_ACCESSORS
#define DEFINE_PRIMITIVE_ACCESSORS(TYPENAME, TYPE, CPPTYPE)        \
  TYPE GeneratedMessageReflection::GetRepeated##TYPENAME(                \
      const Message& message,                                                \
      const FieldDescriptor* field, int index) const {                       \
    if (field->is_extension()) {                                             \
      return TYPE();                                                         \
    } else {                                                                 \
      return GetField<QList<TYPE>>(message, field)[index];                   \
    }                                                                        \
  }                                                                          \

DEFINE_PRIMITIVE_ACCESSORS(Int32 , int32_t , INT32 )
DEFINE_PRIMITIVE_ACCESSORS(Int64 , int64_t , INT64 )
DEFINE_PRIMITIVE_ACCESSORS(UInt32, uint32_t, UINT32)
DEFINE_PRIMITIVE_ACCESSORS(UInt64, uint64_t, UINT64)
DEFINE_PRIMITIVE_ACCESSORS(Float , float   , FLOAT )
DEFINE_PRIMITIVE_ACCESSORS(Double, double  , DOUBLE)
DEFINE_PRIMITIVE_ACCESSORS(Bool  , bool    , BOOL  )
#undef DEFINE_PRIMITIVE_ACCESSORS

} // namespace protobuf_qt
