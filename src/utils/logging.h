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

#ifndef LOGGING_H
#define LOGGING_H

#include <QDebug>

#include <glog/logging.h>
#include <ostream>

namespace {

template<typename T>
inline static std::ostream& StreamQtObject(std::ostream& s, const T& object) {
  QString buf;
  QDebug deb(&buf);
  deb.nospace() << object;
  s << buf.toUtf8().constData();
  return s;
}

}

inline std::ostream& operator <<(std::ostream& s, const QString& o) {
  return StreamQtObject(s, o);
}

inline std::ostream& operator <<(std::ostream& s, const QByteArray& o) {
  return StreamQtObject(s, o);
}

inline std::ostream& operator <<(std::ostream& s, const QVariant& o) {
  return StreamQtObject(s, o);
}

template<typename T>
inline std::ostream& operator <<(std::ostream& s, const QList<T>& o) {
  return StreamQtObject(s, o);
}

template<typename K, typename V>
inline std::ostream& operator <<(std::ostream& s, const QMap<K, V>& o) {
  return StreamQtObject(s, o);
}

#endif // LOGGING_H
