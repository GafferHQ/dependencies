// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains implementation details, the public interface is declared
// in time_format.h.

#ifndef UI_BASE_L10N_FORMATTER_H_
#define UI_BASE_L10N_FORMATTER_H_

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/msgfmt.h"
#include "third_party/icu/source/i18n/unicode/plurrule.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/ui_base_export.h"

namespace ui {

struct Pluralities;

// Formatter for a (format, length) combination.  May either be instantiated
// with four parameters for use in TimeFormat::Simple() or with ten parameters
// for use in TimeFormat::Detailed().
class Formatter {
 public:
  enum Unit {
    UNIT_SEC,
    UNIT_MIN,
    UNIT_HOUR,
    UNIT_DAY,
    UNIT_COUNT  // Enum size counter, not a unit.  Must be last.
  };
  enum TwoUnits {
    TWO_UNITS_MIN_SEC,
    TWO_UNITS_HOUR_MIN,
    TWO_UNITS_DAY_HOUR,
    TWO_UNITS_COUNT      // Enum size counter, not a unit pair.  Must be last.
  };

  Formatter(const Pluralities& sec_pluralities,
            const Pluralities& min_pluralities,
            const Pluralities& hour_pluralities,
            const Pluralities& day_pluralities);

  Formatter(const Pluralities& sec_pluralities,
            const Pluralities& min_pluralities,
            const Pluralities& hour_pluralities,
            const Pluralities& day_pluralities,
            const Pluralities& min_sec_pluralities1,
            const Pluralities& min_sec_pluralities2,
            const Pluralities& hour_min_pluralities1,
            const Pluralities& hour_min_pluralities2,
            const Pluralities& day_hour_pluralities1,
            const Pluralities& day_hour_pluralities2);

  void Format(Unit unit, int value, icu::UnicodeString* formatted_string) const;

  void Format(TwoUnits units,
              int value_1,
              int value_2,
              icu::UnicodeString* formatted_string) const;

 private:
  // Create a hard-coded fallback message format for plural formatting.
  // This will never be called unless translators make a mistake.
  scoped_ptr<icu::MessageFormat> CreateFallbackFormat(
      const icu::PluralRules& rules,
      const Pluralities& pluralities) const;

  scoped_ptr<icu::MessageFormat> InitFormat(const Pluralities& pluralities);

  scoped_ptr<icu::MessageFormat> simple_format_[UNIT_COUNT];
  scoped_ptr<icu::MessageFormat> detailed_format_[TWO_UNITS_COUNT][2];

  DISALLOW_IMPLICIT_CONSTRUCTORS(Formatter);
};

// Class to hold all Formatters, intended to be used in a global LazyInstance.
class UI_BASE_EXPORT FormatterContainer {
 public:
  FormatterContainer();
  ~FormatterContainer();

  const Formatter* Get(TimeFormat::Format format,
                       TimeFormat::Length length) const;

  void ResetForTesting() {
    Shutdown();
    Initialize();
  }

 private:
  void Initialize();
  void Shutdown();

  scoped_ptr<Formatter>
      formatter_[TimeFormat::FORMAT_COUNT][TimeFormat::LENGTH_COUNT];

  DISALLOW_COPY_AND_ASSIGN(FormatterContainer);
};

// Windows compilation requires full definition of FormatterContainer before
// LazyInstance<FormatterContainter> may be declared.
extern UI_BASE_EXPORT base::LazyInstance<FormatterContainer> g_container;

// For use in unit tests only.
extern UI_BASE_EXPORT bool formatter_force_fallback;

}  // namespace ui

#endif  // UI_BASE_L10N_FORMATTER_H_
