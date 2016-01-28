// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include "cpp_string_field.h"
#include "cpp_helpers.h"
#include <google/protobuf/io/printer.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

namespace {

void SetStringVariables(const FieldDescriptor* descriptor,
                        map<string, string>* variables,
                        const Options& options) {
  SetCommonFieldVariables(descriptor, variables, options);
  (*variables)["default"] = DefaultValue(descriptor);
  (*variables)["default_length"] =
      SimpleItoa(descriptor->default_value_string().length());
  (*variables)["default_variable"] = "_default_" + FieldName(descriptor) + "_";
  (*variables)["full_name"] = descriptor->full_name();
  (*variables)["qt_type"] =
      descriptor->type() == FieldDescriptor::TYPE_BYTES ? "QByteArray" : "QString";
}

}  // namespace

// ===================================================================

StringFieldGenerator::
StringFieldGenerator(const FieldDescriptor* descriptor,
                     const Options& options)
  : descriptor_(descriptor) {
  SetStringVariables(descriptor, &variables_, options);
}

StringFieldGenerator::~StringFieldGenerator() {}

void StringFieldGenerator::
GeneratePrivateMembers(io::Printer* printer) const {
  printer->Print(variables_, "$qt_type$ $name$_;\n");
}

void StringFieldGenerator::
GenerateStaticMembers(io::Printer* printer) const {
  if (!descriptor_->default_value_string().empty()) {
    printer->Print(variables_, "static $qt_type$* $default_variable$;\n");
  }
}

void StringFieldGenerator::
GenerateAccessorDeclarations(io::Printer* printer) const {
  // If we're using StringFieldGenerator for a field with a ctype, it's
  // because that ctype isn't actually implemented.  In particular, this is
  // true of ctype=CORD and ctype=STRING_PIECE in the open source release.
  // We aren't releasing Cord because it has too many Google-specific
  // dependencies and we aren't releasing StringPiece because it's hardly
  // useful outside of Google and because it would get confusing to have
  // multiple instances of the StringPiece class in different libraries (PCRE
  // already includes it for their C++ bindings, which came from Google).
  //
  // In any case, we make all the accessors private while still actually
  // using a string to represent the field internally.  This way, we can
  // guarantee that if we do ever implement the ctype, it won't break any
  // existing users who might be -- for whatever reason -- already using .proto
  // files that applied the ctype.  The field can still be accessed via the
  // reflection interface since the reflection interface is independent of
  // the string's underlying representation.
  if (descriptor_->options().ctype() != FieldOptions::STRING) {
    printer->Outdent();
    printer->Print(
      " private:\n"
      "  // Hidden due to unknown ctype option.\n");
    printer->Indent();
  }

  printer->Print(variables_,
    "inline const $qt_type$& $name$() const$deprecation$;\n"
    "inline void set_$name$(const $qt_type$& value)$deprecation$;\n"
    "inline $qt_type$* mutable_$name$()$deprecation$;\n");


  if (descriptor_->options().ctype() != FieldOptions::STRING) {
    printer->Outdent();
    printer->Print(" public:\n");
    printer->Indent();
  }
}

void StringFieldGenerator::
GenerateInlineAccessorDefinitions(io::Printer* printer) const {
  printer->Print(variables_,
    "inline const $qt_type$& $classname$::$name$() const {\n"
    "  // @@protoc_insertion_point(field_get:$full_name$)\n"
    "  return $name$_;\n"
    "}\n"
    "inline void $classname$::set_$name$(const $qt_type$& value) {\n"
    "  set_has_$name$();\n"
    "  $name$_ = value;\n"
    "  // @@protoc_insertion_point(field_set:$full_name$)\n"
    "}\n"
    "inline $qt_type$* $classname$::mutable_$name$() {\n"
    "  set_has_$name$();\n"
    "  // @@protoc_insertion_point(field_mutable:$full_name$)\n"
    "  return &$name$_;\n"
    "}\n");
}

void StringFieldGenerator::
GenerateNonInlineAccessorDefinitions(io::Printer* printer) const {
  if (!descriptor_->default_value_string().empty()) {
    // Initialized in GenerateDefaultInstanceAllocator.
    printer->Print(variables_,
      "$qt_type$* $classname$::$default_variable$ = NULL;\n");
  }
}

void StringFieldGenerator::
GenerateClearingCode(io::Printer* printer) const {
  printer->Print(variables_, "$name$_.clear();\n");
}

void StringFieldGenerator::
GenerateMergingCode(io::Printer* printer) const {
  printer->Print(variables_, "set_$name$(from.$name$());\n");
}

void StringFieldGenerator::
GenerateSwappingCode(io::Printer* printer) const {
  printer->Print(variables_, "std::swap($name$_, other->$name$_);\n");
}

void StringFieldGenerator::
GenerateConstructorCode(io::Printer* printer) const {
  if (!descriptor_->default_value_string().empty()) {
    printer->Print(variables_,
      "$name$_ = *$default_variable$;\n");
  }
}

void StringFieldGenerator::
GenerateDestructorCode(io::Printer* printer) const {
}

void StringFieldGenerator::
GenerateDefaultInstanceAllocator(io::Printer* printer) const {
  if (!descriptor_->default_value_string().empty()) {
    printer->Print(variables_,
      "$classname$::$default_variable$ =\n");
    if (descriptor_->type() == FieldDescriptor::TYPE_BYTES) {
      printer->Print(variables_,
        "  new QByteArray($default$, $default_length$);\n");
    } else {
      printer->Print(variables_,
        "  new QString(QString::fromUtf8($default$, $default_length$));\n");
    }
  }
}

void StringFieldGenerator::
GenerateShutdownCode(io::Printer* printer) const {
}

void StringFieldGenerator::
GenerateMergeFromCodedStream(io::Printer* printer) const {
  printer->Print(variables_,
    "google::protobuf::uint32 length;\n"
    "DO_(input->ReadVarint32(&length));\n");
  if (descriptor_->type() == FieldDescriptor::TYPE_BYTES) {
    printer->Print(variables_,
      "this->mutable_$name$()->resize(length);\n"
      "DO_(input->ReadRaw(this->mutable_$name$()->data(), length));\n");
  } else {
    printer->Print(variables_,
      "QByteArray bytes;\n"
      "bytes.resize(length);\n"
      "DO_(input->ReadRaw(bytes.data(), length));\n"
      "this->set_$name$(QString::fromUtf8(bytes));\n");
  }
}

void StringFieldGenerator::
GenerateSerializeWithCachedSizes(io::Printer* printer) const {
  printer->Print(variables_,
    "::google::protobuf::internal::WireFormatLite::WriteTag(\n"
    "  $number$,\n"
    "  ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED,\n"
    "  output);\n");
  if (descriptor_->type() == FieldDescriptor::TYPE_BYTES) {
    printer->Print(variables_,
      "output->WriteVarint32(this->$name$().count());\n"
      "output->WriteRaw(this->$name$().constData(), this->$name$().count());\n");
  } else {
    printer->Print(variables_,
      "QByteArray bytes(this->$name$().toUtf8());\n"
      "output->WriteVarint32(bytes.count());\n"
      "output->WriteRaw(bytes.constData(), bytes.count());\n");
  }
}

void StringFieldGenerator::
GenerateSerializeWithCachedSizesToArray(io::Printer* printer) const {
  printer->Print(variables_,
    "target = ::google::protobuf::internal::WireFormatLite::WriteTagToArray(\n"
    "  $number$,\n"
    "  ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED,\n"
    "  target);\n");
  if (descriptor_->type() == FieldDescriptor::TYPE_BYTES) {
    printer->Print(variables_,
      "target = ::google::protobuf::io::CodedOutputStream::WriteVarint32ToArray(\n"
      "  this->$name$().count(), target);\n"
      "target = ::google::protobuf::io::CodedOutputStream::WriteRawToArray(\n"
      "  this->$name$().constData(), this->$name$().count(), target);\n");
  } else {
    printer->Print(variables_,
      "QByteArray bytes(this->$name$().toUtf8());\n"
      "target = ::google::protobuf::io::CodedOutputStream::WriteVarint32ToArray(\n"
      "  bytes.count(), target);\n"
      "target = ::google::protobuf::io::CodedOutputStream::WriteRawToArray(\n"
      "  bytes.constData(), bytes.count(), target);\n");
  }
}

void StringFieldGenerator::
GenerateByteSize(io::Printer* printer) const {
  if (descriptor_->type() == FieldDescriptor::TYPE_BYTES) {
    printer->Print(variables_,
      "total_size += $tag_size$ +\n"
      "  ::google::protobuf::io::CodedOutputStream::VarintSize32(this->$name$().count()) +\n"
      "  this->$name$().count();\n");
  } else {
    printer->Print(variables_,
      "QByteArray bytes(this->$name$().toUtf8());\n"
      "total_size += $tag_size$ +\n"
      "  ::google::protobuf::io::CodedOutputStream::VarintSize32(bytes.count()) +\n"
      "  bytes.count();\n");
  }
}

// ===================================================================

StringOneofFieldGenerator::
StringOneofFieldGenerator(const FieldDescriptor* descriptor,
                          const Options& options)
  : StringFieldGenerator(descriptor, options) {
  SetCommonOneofFieldVariables(descriptor, &variables_);
}

StringOneofFieldGenerator::~StringOneofFieldGenerator() {}

void StringOneofFieldGenerator::
GenerateInlineAccessorDefinitions(io::Printer* printer) const {
  printer->Print(variables_,
    "inline const $qt_type$& $classname$::$name$() const {\n"
    "  if (has_$name$()) {\n"
    "    return $oneof_prefix$$name$_;\n"
    "  }\n"
    "  return $default_variable$;\n"
    "}\n"
    "inline void $classname$::set_$name$(const $qt_type$& value) {\n"
    "  if (!has_$name$()) {\n"
    "    clear_$oneof_name$();\n"
    "    set_has_$name$();\n"
    "  }\n"
    "  $oneof_prefix$$name$_ = value;\n"
    "}\n"
    "inline ::std::string* $classname$::mutable_$name$() {\n"
    "  if (!has_$name$()) {\n"
    "    clear_$oneof_name$();\n"
    "    set_has_$name$();\n"
    "    $oneof_prefix$$dname$_ = *$default_variable$;\n"
    "  }\n"
    "  return $oneof_prefix$$name$_;\n"
    "}\n");
}

void StringOneofFieldGenerator::
GenerateClearingCode(io::Printer* printer) const {
}

void StringOneofFieldGenerator::
GenerateSwappingCode(io::Printer* printer) const {
  // Don't print any swapping code. Swapping the union will swap this field.
}

void StringOneofFieldGenerator::
GenerateConstructorCode(io::Printer* printer) const {
  if (!descriptor_->default_value_string().empty()) {
    printer->Print(variables_,
      "  $classname$_default_oneof_instance_->$name$_ = "
      "$classname$::$default_variable$;\n");
  } else {
    printer->Print(variables_,
      "  $classname$_default_oneof_instance_->$name$_ = "
      "$default_variable$;\n");
  }
}

void StringOneofFieldGenerator::
GenerateDestructorCode(io::Printer* printer) const {
}

// ===================================================================

RepeatedStringFieldGenerator::
RepeatedStringFieldGenerator(const FieldDescriptor* descriptor,
                             const Options& options)
  : descriptor_(descriptor) {
  SetStringVariables(descriptor, &variables_, options);
  variables_["list_type"] =
      descriptor->type() == FieldDescriptor::TYPE_BYTES ?
        "QList<QByteArray>" : "QStringList";
}

RepeatedStringFieldGenerator::~RepeatedStringFieldGenerator() {}

void RepeatedStringFieldGenerator::
GeneratePrivateMembers(io::Printer* printer) const {
  printer->Print(variables_,
    "$list_type$ $name$_;\n");
}

void RepeatedStringFieldGenerator::
GenerateAccessorDeclarations(io::Printer* printer) const {
  // See comment above about unknown ctypes.
  if (descriptor_->options().ctype() != FieldOptions::STRING) {
    printer->Outdent();
    printer->Print(
      " private:\n"
      "  // Hidden due to unknown ctype option.\n");
    printer->Indent();
  }

  printer->Print(variables_,
    "inline const $list_type$& $name$() const$deprecation$;\n"
    "inline $list_type$* mutable_$name$()$deprecation$;\n"
    "inline void set_$name$(const $list_type$& value)$deprecation$;\n"
    "inline void add_$name$(const $qt_type$& value)$deprecation$;\n");

  if (descriptor_->options().ctype() != FieldOptions::STRING) {
    printer->Outdent();
    printer->Print(" public:\n");
    printer->Indent();
  }
}

void RepeatedStringFieldGenerator::
GenerateInlineAccessorDefinitions(io::Printer* printer) const {
  printer->Print(variables_,
    "inline const $list_type$& $classname$::$name$() const {\n"
    "  return $name$_;\n"
    "}\n"
    "inline $list_type$* $classname$::mutable_$name$() {\n"
    "  return &$name$_;\n"
    "}\n"
    "inline void $classname$::set_$name$(const $list_type$& value) {\n"
    "  $name$_ = value;\n"
    "}\n"
    "inline void $classname$::add_$name$(const $qt_type$& value) {\n"
    "  $name$_.append(value);\n"
    "}\n");
}

void RepeatedStringFieldGenerator::
GenerateClearingCode(io::Printer* printer) const {
  printer->Print(variables_, "$name$_.clear();\n");
}

void RepeatedStringFieldGenerator::
GenerateMergingCode(io::Printer* printer) const {
  printer->Print(variables_, "$name$_.append(from.$name$_);\n");
}

void RepeatedStringFieldGenerator::
GenerateSwappingCode(io::Printer* printer) const {
  printer->Print(variables_, "$name$_.swap(other->$name$_);\n");
}

void RepeatedStringFieldGenerator::
GenerateConstructorCode(io::Printer* printer) const {
  // Not needed for repeated fields.
}

void RepeatedStringFieldGenerator::
GenerateMergeFromCodedStream(io::Printer* printer) const {
  printer->Print(variables_,
    "google::protobuf::uint32 length;\n"
    "DO_(input->ReadVarint32(&length));\n"
    "QByteArray bytes;\n"
    "bytes.resize(length);\n"
    "DO_(input->ReadRaw(bytes.data(), length));\n");
  if (descriptor_->type() == FieldDescriptor::TYPE_BYTES) {
    printer->Print(variables_,
      "this->add_$name$(bytes);\n");
  } else {
    printer->Print(variables_,
      "this->add_$name$(QString::fromUtf8(bytes));\n");
  }
}

void RepeatedStringFieldGenerator::
GenerateSerializeWithCachedSizes(io::Printer* printer) const {
  printer->Print(variables_,
    "for (int i = 0; i < this->$name$().count(); i++) {\n"
    "  ::google::protobuf::internal::WireFormatLite::WriteTag(\n"
    "    $number$,\n"
    "    ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED,\n"
    "    output);\n");
  if (descriptor_->type() == FieldDescriptor::TYPE_BYTES) {
    printer->Print(variables_,
      "  output->WriteVarint32(this->$name$()[i].count());\n"
      "  output->WriteRaw(this->$name$()[i].constData(), this->$name$()[i].count());\n");
  } else {
    printer->Print(variables_,
      "  QByteArray bytes(this->$name$()[i].toUtf8());\n"
      "  output->WriteVarint32(bytes.count());\n"
      "  output->WriteRaw(bytes.constData(), bytes.count());\n");
  }
  printer->Print(variables_, "}\n");
}

void RepeatedStringFieldGenerator::
GenerateSerializeWithCachedSizesToArray(io::Printer* printer) const {
  printer->Print(variables_,
    "for (int i = 0; i < this->$name$().count(); i++) {\n"
    "  target = ::google::protobuf::internal::WireFormatLite::WriteTagToArray(\n"
    "    $number$,\n"
    "    ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED,\n"
    "    target);\n");
  if (descriptor_->type() == FieldDescriptor::TYPE_BYTES) {
    printer->Print(variables_,
      "  target = ::google::protobuf::io::CodedOutputStream::WriteVarint32ToArray(\n"
      "    this->$name$()[i].count(), target);\n"
      "  target = ::google::protobuf::io::CodedOutputStream::WriteRawToArray(\n"
      "    this->$name$()[i].constData(), this->$name$()[i].count(), target);\n");
  } else {
    printer->Print(variables_,
      "  QByteArray bytes(this->$name$()[i].toUtf8());\n"
      "  target = ::google::protobuf::io::CodedOutputStream::WriteVarint32ToArray(\n"
      "    bytes.count(), target);\n"
      "  target = ::google::protobuf::io::CodedOutputStream::WriteRawToArray(\n"
      "    bytes.constData(), bytes.count(), target);\n");
  }
  printer->Print(variables_, "}\n");
}

void RepeatedStringFieldGenerator::
GenerateByteSize(io::Printer* printer) const {
  printer->Print(variables_,
    "total_size += $tag_size$ * this->$name$().count();\n"
    "for (int i = 0; i < this->$name$().count(); i++) {\n");
  if (descriptor_->type() == FieldDescriptor::TYPE_BYTES) {
    printer->Print(variables_,
      "  total_size +=\n"
      "    ::google::protobuf::io::CodedOutputStream::VarintSize32(this->$name$()[i].count()) +\n"
      "    this->$name$()[i].count();\n");
  } else {
    printer->Print(variables_,
      "  QByteArray bytes(this->$name$()[i].toUtf8());\n"
      "  total_size +=\n"
      "    ::google::protobuf::io::CodedOutputStream::VarintSize32(bytes.count()) +\n"
      "    bytes.count();\n");
  }
  printer->Print(variables_, "}\n");
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
