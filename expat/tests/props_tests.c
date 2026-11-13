/* Tests related to the public properties API of Expat
__  __            _
                         ___\ \/ /_ __   __ _| |_
                        / _ \\  /| '_ \ / _` | __|
                       |  __//  \| |_) | (_| | |_
                        \___/_/\_\ .__/ \__,_|\__|
                                 |_| XML parser

   Copyright (c) 2026 Sebastian Pipping <sebastian@pipping.org>
   Licensed under the MIT license:

   Permission is  hereby granted,  free of charge,  to any  person obtaining
   a  copy  of  this  software   and  associated  documentation  files  (the
   "Software"),  to  deal in  the  Software  without restriction,  including
   without  limitation the  rights  to use,  copy,  modify, merge,  publish,
   distribute, sublicense, and/or sell copies of the Software, and to permit
   persons  to whom  the Software  is  furnished to  do so,  subject to  the
   following conditions:

   The above copyright  notice and this permission notice  shall be included
   in all copies or substantial portions of the Software.

   THE  SOFTWARE  IS  PROVIDED  "AS  IS",  WITHOUT  WARRANTY  OF  ANY  KIND,
   EXPRESS  OR IMPLIED,  INCLUDING  BUT  NOT LIMITED  TO  THE WARRANTIES  OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
   NO EVENT SHALL THE AUTHORS OR  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
   DAMAGES OR  OTHER LIABILITY, WHETHER  IN AN  ACTION OF CONTRACT,  TORT OR
   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
   USE OR OTHER DEALINGS IN THE SOFTWARE.

   SPDX-License-Identifier: MIT
*/

#include "expat_config.h"

#include "props_tests.h"

#include "common.h" // for g_chunkSize
#include "expat.h"
#include "internal.h" // for e.g. EXPAT_ALLOC_TRACKER_ACTIVATION_THRESHOLD_DEFAULT

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

struct TagDepthCounters {
  int startElementEvents;
  int endElementEvents;
};

static void XMLCALL
tag_depth_start_handler(void *userData, const XML_Char *name,
                        const XML_Char **atts) {
  struct TagDepthCounters *const counters = userData;
  UNUSED_P(name);
  UNUSED_P(atts);
  counters->startElementEvents++;
}

static void XMLCALL
tag_depth_end_handler(void *userData, const XML_Char *name) {
  struct TagDepthCounters *const counters = userData;
  UNUSED_P(name);
  counters->endElementEvents++;
}

// Build a document with the given number of nested elements.
static char *
make_nested_document(unsigned int depth) {
  static char document[4096];
  size_t offset = 0;
  for (unsigned int level = 0; level < depth; level++) {
    int written = snprintf(document + offset, sizeof(document) - offset,
                           "<a%u>", level);
    assert_true(written > 0);
    assert_true((size_t)written < sizeof(document) - offset);
    offset += (size_t)written;
  }
  for (unsigned int level = depth; level > 0; level--) {
    int written = snprintf(document + offset, sizeof(document) - offset,
                           "</a%u>", level - 1);
    assert_true(written > 0);
    assert_true((size_t)written < sizeof(document) - offset);
    offset += (size_t)written;
  }
  return document;
}

struct ExternalEntityDepthData {
  const char *entityText;
  enum XML_Error expectedError;
};

static int XMLCALL
tag_depth_external_entity_handler(XML_Parser parser, const XML_Char *context,
                                  const XML_Char *base,
                                  const XML_Char *systemId,
                                  const XML_Char *publicId) {
  struct ExternalEntityDepthData *const data = XML_GetUserData(parser);
  UNUSED_P(base);
  UNUSED_P(systemId);
  UNUSED_P(publicId);

  XML_Parser subParser
      = XML_ExternalEntityParserCreate(parser, context, NULL);
  assert_true(subParser != NULL);

  enum XML_Status status
      = XML_Parse_SINGLE_BYTES(subParser, data->entityText,
                               (int)strlen(data->entityText), XML_TRUE);
  const enum XML_Error errorCode = XML_GetErrorCode(subParser);
  XML_ParserFree(subParser);

  if (data->expectedError == XML_ERROR_NONE) {
    assert_true(status == XML_STATUS_OK);
    assert_true(errorCode == XML_ERROR_NONE);
    return XML_STATUS_OK;
  }
  assert_true(status == XML_STATUS_ERROR);
  assert_true(errorCode == data->expectedError);
  return XML_STATUS_ERROR;
}

enum ExpectedType {
  TYPE_BOOL,
  TYPE_DOUBLE,
  TYPE_UINT64,
};

START_TEST(test_props_getter_defaults) {
  // The test is not doing any parsing, so a single run
  // (with `g_chunkSize == 0`) is enough
  if (g_chunkSize != 0)
    return;

  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);

#if XML_GE == 1
  // Case XML_PROP_ALLOC_TRACKER_ACTIVATION_THRESHOLD
  {
    uint64_t actionThresholdBytes1 = 123;
    assert_true(XML_GetPropertyUInt64(
                    parser, XML_PROP_ALLOC_TRACKER_ACTIVATION_THRESHOLD,
                    &actionThresholdBytes1)
                == XML_PROP_ERROR_NONE);
    assert_true(actionThresholdBytes1
                == EXPAT_ALLOC_TRACKER_ACTIVATION_THRESHOLD_DEFAULT);
  }

  // Case XML_PROP_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION
  {
    double maximumAmplification1 = 123.456;
    assert_true(XML_GetPropertyDouble(
                    parser, XML_PROP_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION,
                    &maximumAmplification1)
                == XML_PROP_ERROR_NONE);
    assert_true(maximumAmplification1
                == (double)EXPAT_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION_DEFAULT);
  }

  // Case XML_PROP_BILLION_LAUGHS_ACTIVATION_THRESHOLD
  {
    uint64_t actionThresholdBytes2 = 123;
    assert_true(XML_GetPropertyUInt64(
                    parser, XML_PROP_BILLION_LAUGHS_ACTIVATION_THRESHOLD,
                    &actionThresholdBytes2)
                == XML_PROP_ERROR_NONE);
    assert_true(
        actionThresholdBytes2
        == EXPAT_BILLION_LAUGHS_ATTACK_PROTECTION_ACTIVATION_THRESHOLD_DEFAULT);
  }

  // Case XML_PROP_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION
  {
    double maximumAmplification2 = 123.456;
    assert_true(XML_GetPropertyDouble(
                    parser, XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION,
                    &maximumAmplification2)
                == XML_PROP_ERROR_NONE);
    assert_true(
        maximumAmplification2
        == (double)
            EXPAT_BILLION_LAUGHS_ATTACK_PROTECTION_MAXIMUM_AMPLIFICATION_DEFAULT);
  }
#endif

  // Case XML_PROP_REPARSE_DEFERRAL_ENABLED
  XML_Bool reparseDeferralEnabled = XML_FALSE;
  assert_true(XML_GetPropertyBool(parser, XML_PROP_REPARSE_DEFERRAL_ENABLED,
                                  &reparseDeferralEnabled)
              == XML_PROP_ERROR_NONE);
  assert_true(reparseDeferralEnabled == g_reparseDeferralEnabledDefault);

  // Case XML_PROP_MAXIMUM_TAG_DEPTH (defaults to "disabled", i.e. 0)
  uint64_t maximumTagDepth = 123;
  assert_true(XML_GetPropertyUInt64(parser, XML_PROP_MAXIMUM_TAG_DEPTH,
                                    &maximumTagDepth)
              == XML_PROP_ERROR_NONE);
  assert_true(maximumTagDepth == 0);

  // Case XML_PROP_INVALID
  XML_Bool dummyBool = XML_FALSE;
  double dummyDouble = 123.456;
  uint64_t dummyUInt64 = 123;
  assert_true(XML_GetPropertyBool(parser, XML_PROP_INVALID, &dummyBool)
              == XML_PROP_ERROR_INVALID_KEY);
  assert_true(XML_GetPropertyDouble(parser, XML_PROP_INVALID, &dummyDouble)
              == XML_PROP_ERROR_INVALID_KEY);
  assert_true(XML_GetPropertyUInt64(parser, XML_PROP_INVALID, &dummyUInt64)
              == XML_PROP_ERROR_INVALID_KEY);

  XML_ParserFree(parser);
}
END_TEST

START_TEST(test_props_getter_error_parser_null) {
  // The test is not doing any parsing, so a single run
  // (with `g_chunkSize == 0`) is enough
  if (g_chunkSize != 0)
    return;

  XML_Parser parserNonNull = XML_ParserCreate(NULL);
  XML_Parser parsers[] = {parserNonNull, NULL};
  assert_true(parserNonNull != NULL);

  for (size_t i = 0; i < sizeof(parsers) / sizeof(parsers[0]); i++) {
    XML_Parser parser = parsers[i];
    const enum XML_Prop_Error validKeyExpectedError
        = (parser == NULL) ? XML_PROP_ERROR_PARSER_NULL : XML_PROP_ERROR_NONE;
    // NOTE: Currently the parser is checked for being `NULL` before the key is
    //       being checked for being valid.
    //       That precedence among errors is not considered part of the API
    //       contract: either error would be fine to return.
    const enum XML_Prop_Error invalidKeyExpectedError
        = (parser == NULL) ? XML_PROP_ERROR_PARSER_NULL
                           : XML_PROP_ERROR_INVALID_KEY;

    XML_Bool dummyBool = XML_FALSE;
    double dummyDouble = 123.456;
    uint64_t dummyUInt64 = 123;

#if XML_GE == 1
    assert_true(
        XML_GetPropertyUInt64(
            parser, XML_PROP_ALLOC_TRACKER_ACTIVATION_THRESHOLD, &dummyUInt64)
        == validKeyExpectedError);
    assert_true(
        XML_GetPropertyDouble(
            parser, XML_PROP_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION, &dummyDouble)
        == validKeyExpectedError);
    assert_true(
        XML_GetPropertyUInt64(
            parser, XML_PROP_BILLION_LAUGHS_ACTIVATION_THRESHOLD, &dummyUInt64)
        == validKeyExpectedError);
    assert_true(
        XML_GetPropertyDouble(
            parser, XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION, &dummyDouble)
        == validKeyExpectedError);
#endif
    assert_true(XML_GetPropertyBool(parser, XML_PROP_REPARSE_DEFERRAL_ENABLED,
                                    &dummyBool)
                == validKeyExpectedError);

    assert_true(XML_GetPropertyBool(parser, XML_PROP_INVALID, &dummyBool)
                == invalidKeyExpectedError);
    assert_true(XML_GetPropertyDouble(parser, XML_PROP_INVALID, &dummyDouble)
                == invalidKeyExpectedError);
    assert_true(XML_GetPropertyUInt64(parser, XML_PROP_INVALID, &dummyUInt64)
                == invalidKeyExpectedError);
  }

  XML_ParserFree(parserNonNull);
}
END_TEST

START_TEST(test_props_getter_error_invalid_key) {
  // The test is not doing any parsing, so a single run
  // (with `g_chunkSize == 0`) is enough
  if (g_chunkSize != 0)
    return;

  XML_Bool dummyBool = XML_FALSE;
  double dummyDouble = 123.456;
  uint64_t dummyUInt64 = 123;

  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);

  assert_true(XML_GetPropertyBool(parser, XML_PROP_INVALID, &dummyBool)
              == XML_PROP_ERROR_INVALID_KEY);
  assert_true(XML_GetPropertyDouble(parser, XML_PROP_INVALID, &dummyDouble)
              == XML_PROP_ERROR_INVALID_KEY);
  assert_true(XML_GetPropertyUInt64(parser, XML_PROP_INVALID, &dummyUInt64)
              == XML_PROP_ERROR_INVALID_KEY);

  XML_ParserFree(parser);
}
END_TEST

START_TEST(test_props_getter_error_invalid_type) {
  // The test is not doing any parsing, so a single run
  // (with `g_chunkSize == 0`) is enough
  if (g_chunkSize != 0)
    return;

  struct TestCase {
    enum XML_Parser_Property key;
    enum ExpectedType expectedType;
  };

  struct TestCase cases[] = {
#if XML_GE == 1
      {XML_PROP_ALLOC_TRACKER_ACTIVATION_THRESHOLD, TYPE_UINT64},
      {XML_PROP_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION, TYPE_DOUBLE},
      {XML_PROP_BILLION_LAUGHS_ACTIVATION_THRESHOLD, TYPE_UINT64},
      {XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION, TYPE_DOUBLE},
#endif
      {XML_PROP_REPARSE_DEFERRAL_ENABLED, TYPE_BOOL},
      {XML_PROP_MAXIMUM_TAG_DEPTH, TYPE_UINT64},
  };

  XML_Bool dummyBool = XML_FALSE;
  double dummyDouble = 123.456;
  uint64_t dummyUInt64 = 123;

  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    struct TestCase *const testCase = cases + i;
    set_subtest("property %d", (int)testCase->key);

    const enum XML_Prop_Error expectedBoolError
        = (testCase->expectedType == TYPE_BOOL) ? XML_PROP_ERROR_NONE
                                                : XML_PROP_ERROR_INVALID_TYPE;
    const enum XML_Prop_Error expectedDoubleError
        = (testCase->expectedType == TYPE_DOUBLE) ? XML_PROP_ERROR_NONE
                                                  : XML_PROP_ERROR_INVALID_TYPE;
    const enum XML_Prop_Error expectedUInt64Error
        = (testCase->expectedType == TYPE_UINT64) ? XML_PROP_ERROR_NONE
                                                  : XML_PROP_ERROR_INVALID_TYPE;

    assert_true(XML_GetPropertyBool(parser, testCase->key, &dummyBool)
                == expectedBoolError);
    assert_true(XML_GetPropertyDouble(parser, testCase->key, &dummyDouble)
                == expectedDoubleError);
    assert_true(XML_GetPropertyUInt64(parser, testCase->key, &dummyUInt64)
                == expectedUInt64Error);
  }

  XML_ParserFree(parser);
}
END_TEST

START_TEST(test_props_getter_error_invalid_value) {
  // The test is not doing any parsing, so a single run
  // (with `g_chunkSize == 0`) is enough
  if (g_chunkSize != 0)
    return;

  enum XML_Parser_Property keys[] = {
#if XML_GE == 1
      XML_PROP_ALLOC_TRACKER_ACTIVATION_THRESHOLD,
      XML_PROP_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION,
      XML_PROP_BILLION_LAUGHS_ACTIVATION_THRESHOLD,
      XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION,
#endif
      XML_PROP_REPARSE_DEFERRAL_ENABLED,
  };

  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);

  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
    const enum XML_Parser_Property key = keys[i];
    set_subtest("property %d", (int)key);

    // NOTE: Currently the value output pointer is checked for being `NULL`
    //       before the key is being checked for being valid.
    //       That precedence among errors is not considered part of the API
    //       contract: either error would be fine to return.
    assert_true(XML_GetPropertyBool(parser, key, NULL)
                == XML_PROP_ERROR_INVALID_VALUE);
    assert_true(XML_GetPropertyDouble(parser, key, NULL)
                == XML_PROP_ERROR_INVALID_VALUE);
    assert_true(XML_GetPropertyUInt64(parser, key, NULL)
                == XML_PROP_ERROR_INVALID_VALUE);
  }

  XML_ParserFree(parser);
}
END_TEST

START_TEST(test_props_setter_effective) {
  // The test is not doing any parsing, so a single run
  // (with `g_chunkSize == 0`) is enough
  if (g_chunkSize != 0)
    return;

  struct TestCase {
    enum XML_Parser_Property key;
    enum ExpectedType expectedType;
  };

  struct TestCase cases[] = {
#if XML_GE == 1
      {XML_PROP_ALLOC_TRACKER_ACTIVATION_THRESHOLD, TYPE_UINT64},
      {XML_PROP_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION, TYPE_DOUBLE},
      {XML_PROP_BILLION_LAUGHS_ACTIVATION_THRESHOLD, TYPE_UINT64},
      {XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION, TYPE_DOUBLE},
#endif
      {XML_PROP_REPARSE_DEFERRAL_ENABLED, TYPE_BOOL},
      {XML_PROP_MAXIMUM_TAG_DEPTH, TYPE_UINT64},
  };

  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    struct TestCase *const testCase = cases + i;
    set_subtest("property %d", (int)testCase->key);

    switch (testCase->expectedType) {
    case TYPE_BOOL: {
      // Get original value
      XML_Bool valueOne = ! g_reparseDeferralEnabledDefault;
      assert_true(XML_GetPropertyBool(parser, testCase->key, &valueOne)
                  == XML_PROP_ERROR_NONE);
      const XML_Bool valueTwo = ((valueOne == XML_TRUE) ? XML_FALSE : XML_TRUE);
      assert_true(valueTwo != valueOne); // self-test

      // Test: Set with reported success
      assert_true(XML_SetPropertyBool(parser, testCase->key, valueTwo)
                  == XML_PROP_ERROR_NONE);
      XML_Bool valueThree = valueOne;
      assert_true(valueThree != valueTwo); // self-test

      // Test: Whether the new value has become effective
      assert_true(XML_GetPropertyBool(parser, testCase->key, &valueThree)
                  == XML_PROP_ERROR_NONE);
      assert_true(valueThree == valueTwo);
      break;
    }
    case TYPE_DOUBLE: {
      // Get original value
      double valueOne = 1.23;
      assert_true(XML_GetPropertyDouble(parser, testCase->key, &valueOne)
                  == XML_PROP_ERROR_NONE);
      const double valueTwo = 4.56;
      assert_true(valueTwo != valueOne); // self-test

      // Test: Set with reported success
      assert_true(XML_SetPropertyDouble(parser, testCase->key, valueTwo)
                  == XML_PROP_ERROR_NONE);
      double valueThree = 7.89;
      assert_true(valueThree != valueTwo); // self-test

      // Test: Whether the new value has become effective
      assert_true(XML_GetPropertyDouble(parser, testCase->key, &valueThree)
                  == XML_PROP_ERROR_NONE);
      assert_true((float)valueThree == (float)valueTwo);
      break;
    }
    case TYPE_UINT64: {
      // Get original value
      uint64_t valueOne = 123;
      assert_true(XML_GetPropertyUInt64(parser, testCase->key, &valueOne)
                  == XML_PROP_ERROR_NONE);
      const uint64_t valueTwo = 456;
      assert_true(valueTwo != valueOne); // self-test

      // Test: Set with reported success
      assert_true(XML_SetPropertyUInt64(parser, testCase->key, valueTwo)
                  == XML_PROP_ERROR_NONE);
      uint64_t valueThree = 789;
      assert_true(valueThree != valueTwo); // self-test

      // Test: Whether the new value has become effective
      assert_true(XML_GetPropertyUInt64(parser, testCase->key, &valueThree)
                  == XML_PROP_ERROR_NONE);
      assert_true(valueThree == valueTwo);
      break;
    }
    default:
      fail("unsupported type");
    }
  }

  XML_ParserFree(parser);
}
END_TEST

START_TEST(test_props_setter_error_parser_null) {
  // The test is not doing any parsing, so a single run
  // (with `g_chunkSize == 0`) is enough
  if (g_chunkSize != 0)
    return;

  struct TestCase {
    enum XML_Parser_Property key;
    enum ExpectedType expectedType;
  };

  struct TestCase cases[] = {
#if XML_GE == 1
      {XML_PROP_ALLOC_TRACKER_ACTIVATION_THRESHOLD, TYPE_UINT64},
      {XML_PROP_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION, TYPE_DOUBLE},
      {XML_PROP_BILLION_LAUGHS_ACTIVATION_THRESHOLD, TYPE_UINT64},
      {XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION, TYPE_DOUBLE},
#endif
      {XML_PROP_REPARSE_DEFERRAL_ENABLED, TYPE_BOOL},
      {XML_PROP_MAXIMUM_TAG_DEPTH, TYPE_UINT64},
  };

  XML_Parser parserNonNull = XML_ParserCreate(NULL);
  XML_Parser parsers[] = {parserNonNull, NULL};
  assert_true(parserNonNull != NULL);

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    struct TestCase *const testCase = cases + i;
    for (size_t j = 0; j < sizeof(parsers) / sizeof(parsers[0]); j++) {
      XML_Parser parser = parsers[j];
      set_subtest("property %d, parser %s", (int)testCase->key,
                  (parser == NULL) ? "NULL" : "non-NULL");

      const enum XML_Prop_Error expected
          = ((parser == NULL) ? XML_PROP_ERROR_PARSER_NULL
                              : XML_PROP_ERROR_NONE);

      enum XML_Prop_Error actual
          = XML_PROP_ERROR_INVALID_TYPE; // i.e. some value unequal to
                                         // `expected`
      // above
      assert_true(actual != expected); // self-test

      switch (testCase->expectedType) {
      case TYPE_BOOL:
        actual = XML_SetPropertyBool(parser, testCase->key,
                                     ! g_reparseDeferralEnabledDefault);
        break;
      case TYPE_DOUBLE:
        actual = XML_SetPropertyDouble(parser, testCase->key, 456.789);
        break;
      case TYPE_UINT64:
        actual = XML_SetPropertyUInt64(parser, testCase->key, 456);
        break;
      default:
        fail("unsupported type");
      }
      assert_true(actual == expected);
    }
  }

  XML_ParserFree(parserNonNull);
}
END_TEST

START_TEST(test_props_setter_error_parser_not_root) {
  // The test is not doing any parsing, so a single run
  // (with `g_chunkSize == 0`) is enough
  if (g_chunkSize != 0)
    return;

  struct TestCase {
    enum XML_Parser_Property key;
    enum ExpectedType expectedType;
    bool needsRootParser;
  };

  struct TestCase cases[] = {
#if XML_GE == 1
      {XML_PROP_ALLOC_TRACKER_ACTIVATION_THRESHOLD, TYPE_UINT64, true},
      {XML_PROP_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION, TYPE_DOUBLE, true},
      {XML_PROP_BILLION_LAUGHS_ACTIVATION_THRESHOLD, TYPE_UINT64, true},
      {XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION, TYPE_DOUBLE, true},
#endif
      {XML_PROP_REPARSE_DEFERRAL_ENABLED, TYPE_BOOL, false},
      {XML_PROP_MAXIMUM_TAG_DEPTH, TYPE_UINT64, true},
  };

  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);
  XML_Parser subParser = XML_ExternalEntityParserCreate(parser, NULL, NULL);
#if defined(XML_DTD)
  assert_true(subParser != NULL);

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    struct TestCase *const testCase = cases + i;
    set_subtest("property %d", (int)testCase->key);

    const enum XML_Prop_Error expected
        = (testCase->needsRootParser ? XML_PROP_ERROR_PARSER_NOT_ROOT
                                     : XML_PROP_ERROR_NONE);

    enum XML_Prop_Error actual
        = XML_PROP_ERROR_INVALID_TYPE; // i.e. some value unequal to `expected`
                                       // above
    assert_true(actual != expected);   // self-test

    switch (testCase->expectedType) {
    case TYPE_BOOL:
      actual = XML_SetPropertyBool(subParser, testCase->key,
                                   ! g_reparseDeferralEnabledDefault);
      break;
    case TYPE_DOUBLE:
      actual = XML_SetPropertyDouble(subParser, testCase->key, 456.789);
      break;
    case TYPE_UINT64:
      actual = XML_SetPropertyUInt64(subParser, testCase->key, 456);
      break;
    default:
      fail("unsupported type");
    }

    assert_true(actual == expected);
  }

  XML_ParserFree(subParser);
#else // ! defined(XML_DTD)
  assert_true(subParser == NULL);
  UNUSED_P(cases);
#endif
  XML_ParserFree(parser);
}
END_TEST

START_TEST(test_props_setter_error_parser_started) {
  // The test is not doing any (real) parsing, so a single run
  // (with `g_chunkSize == 0`) is enough
  if (g_chunkSize != 0)
    return;

  struct TestCase {
    enum XML_Parser_Property key;
    enum ExpectedType expectedType;
  };

  struct TestCase cases[] = {
#if XML_GE == 1
      {XML_PROP_ALLOC_TRACKER_ACTIVATION_THRESHOLD, TYPE_UINT64},
      {XML_PROP_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION, TYPE_DOUBLE},
      {XML_PROP_BILLION_LAUGHS_ACTIVATION_THRESHOLD, TYPE_UINT64},
      {XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION, TYPE_DOUBLE},
#endif
      {XML_PROP_REPARSE_DEFERRAL_ENABLED, TYPE_BOOL},
      {XML_PROP_MAXIMUM_TAG_DEPTH, TYPE_UINT64},
  };

  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);

  // Get the parser into state "parsing started"
  XML_ParsingStatus parsingStatus;
  XML_GetParsingStatus(parser, &parsingStatus);
  assert_true(parsingStatus.parsing == XML_INITIALIZED); // Self-test
  assert_true(XML_Parse_SINGLE_BYTES(parser, "", 0, /*isFInal=*/XML_FALSE)
              == XML_STATUS_OK);
  XML_GetParsingStatus(parser, &parsingStatus);
  assert_true(parsingStatus.parsing == XML_PARSING);

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    struct TestCase *const testCase = cases + i;
    set_subtest("property %d", (int)testCase->key);

    switch (testCase->expectedType) {
    case TYPE_BOOL: {
      // Get original value
      XML_Bool valueOne = ! g_reparseDeferralEnabledDefault;
      assert_true(XML_GetPropertyBool(parser, testCase->key, &valueOne)
                  == XML_PROP_ERROR_NONE);
      const XML_Bool valueTwo = ((valueOne == XML_TRUE) ? XML_FALSE : XML_TRUE);
      assert_true(valueTwo != valueOne); // self-test

      // Test: Set with reported success
      assert_true(XML_SetPropertyBool(parser, testCase->key, valueTwo)
                  == XML_PROP_ERROR_NONE);
      XML_Bool valueThree = valueOne;
      assert_true(valueThree != valueTwo); // self-test

      // Test: Whether the new value has become effective
      assert_true(XML_GetPropertyBool(parser, testCase->key, &valueThree)
                  == XML_PROP_ERROR_NONE);
      assert_true(valueThree == valueTwo); // i.e. new value applied
      break;
    }
    case TYPE_DOUBLE: {
      // Get original value
      double valueOne = 1.23;
      assert_true(XML_GetPropertyDouble(parser, testCase->key, &valueOne)
                  == XML_PROP_ERROR_NONE);
      const double valueTwo = 4.56;
      assert_true(valueTwo != valueOne); // self-test

      // Test: Set with reported success
      assert_true(XML_SetPropertyDouble(parser, testCase->key, valueTwo)
                  == XML_PROP_ERROR_NONE);
      double valueThree = 7.89;
      assert_true(valueThree != valueTwo); // self-test

      // Test: Whether the new value has become effective
      assert_true(XML_GetPropertyDouble(parser, testCase->key, &valueThree)
                  == XML_PROP_ERROR_NONE);
      assert_true((float)valueThree
                  == (float)valueTwo); // i.e. new value applied
      break;
    }
    case TYPE_UINT64: {
      // Get original value
      uint64_t valueOne = 123;
      assert_true(XML_GetPropertyUInt64(parser, testCase->key, &valueOne)
                  == XML_PROP_ERROR_NONE);
      const uint64_t valueTwo = 456;
      assert_true(valueTwo != valueOne); // self-test

      // Test: Set with reported success
      assert_true(XML_SetPropertyUInt64(parser, testCase->key, valueTwo)
                  == XML_PROP_ERROR_NONE);
      uint64_t valueThree = 789;
      assert_true(valueThree != valueTwo); // self-test

      // Test: Whether the new value has become effective
      assert_true(XML_GetPropertyUInt64(parser, testCase->key, &valueThree)
                  == XML_PROP_ERROR_NONE);
      assert_true(valueThree == valueTwo); // i.e. new value applied
      break;
    }
    default:
      fail("unsupported type");
    }
  }

  XML_ParserFree(parser);
}
END_TEST

START_TEST(test_props_setter_error_invalid_key) {
  // The test is not doing any parsing, so a single run
  // (with `g_chunkSize == 0`) is enough
  if (g_chunkSize != 0)
    return;

  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);

  assert_true(XML_SetPropertyBool(parser, XML_PROP_INVALID,
                                  ! g_reparseDeferralEnabledDefault)
              == XML_PROP_ERROR_INVALID_KEY);
  assert_true(XML_SetPropertyDouble(parser, XML_PROP_INVALID, 123.456)
              == XML_PROP_ERROR_INVALID_KEY);
  assert_true(XML_SetPropertyUInt64(parser, XML_PROP_INVALID, 123)
              == XML_PROP_ERROR_INVALID_KEY);

  XML_ParserFree(parser);
}
END_TEST

START_TEST(test_props_setter_error_invalid_type) {
  // The test is not doing any parsing, so a single run
  // (with `g_chunkSize == 0`) is enough
  if (g_chunkSize != 0)
    return;

  struct TestCase {
    enum XML_Parser_Property key;
    enum ExpectedType expectedType;
  };

  struct TestCase cases[] = {
#if XML_GE == 1
      {XML_PROP_ALLOC_TRACKER_ACTIVATION_THRESHOLD, TYPE_UINT64},
      {XML_PROP_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION, TYPE_DOUBLE},
      {XML_PROP_BILLION_LAUGHS_ACTIVATION_THRESHOLD, TYPE_UINT64},
      {XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION, TYPE_DOUBLE},
#endif
      {XML_PROP_REPARSE_DEFERRAL_ENABLED, TYPE_BOOL},
      {XML_PROP_MAXIMUM_TAG_DEPTH, TYPE_UINT64},
  };

  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    struct TestCase *const testCase = cases + i;
    set_subtest("property %d", (int)testCase->key);

    const enum XML_Prop_Error expectedErrorBool
        = ((testCase->expectedType == TYPE_BOOL) ? XML_PROP_ERROR_NONE
                                                 : XML_PROP_ERROR_INVALID_TYPE);
    const enum XML_Prop_Error expectedErrorDouble
        = ((testCase->expectedType == TYPE_DOUBLE)
               ? XML_PROP_ERROR_NONE
               : XML_PROP_ERROR_INVALID_TYPE);
    const enum XML_Prop_Error expectedErrorUInt64
        = ((testCase->expectedType == TYPE_UINT64)
               ? XML_PROP_ERROR_NONE
               : XML_PROP_ERROR_INVALID_TYPE);

    assert_true(XML_SetPropertyBool(parser, testCase->key,
                                    ! g_reparseDeferralEnabledDefault)
                == expectedErrorBool);
    assert_true(XML_SetPropertyDouble(parser, testCase->key, 456.789)
                == expectedErrorDouble);
    assert_true(XML_SetPropertyUInt64(parser, testCase->key, 456)
                == expectedErrorUInt64);
  }

  XML_ParserFree(parser);
}
END_TEST

START_TEST(test_props_setter_error_invalid_value) {
  // The test is not doing any parsing, so a single run
  // (with `g_chunkSize == 0`) is enough
  if (g_chunkSize != 0)
    return;

  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);

#if XML_GE == 1
  struct TestCase {
    enum XML_Parser_Property key;
    // NOTE: Neither `uint64_t`- nor `XML_Bool`-based properties have any
    //       invalid in-dimension values, so we are only testing `double`-based
    //       property values here.
    double value;
    bool valueValid;
  };

  struct TestCase cases[] = {
      // XML_PROP_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION
      {XML_PROP_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION, -HUGE_VAL, false},
      {XML_PROP_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION, -1.0, false},
      {XML_PROP_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION, -0.9, false},
      {XML_PROP_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION, 0.0, false},
      {XML_PROP_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION, 0.9, false},
      {XML_PROP_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION, 1.0, true},
      {XML_PROP_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION, HUGE_VAL, true},
      // XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION
      {XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION, -HUGE_VAL, false},
      {XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION, -1.0, false},
      {XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION, -0.9, false},
      {XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION, 0.0, false},
      {XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION, 0.9, false},
      {XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION, 1.0, true},
      {XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION, HUGE_VAL, true},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    struct TestCase *const testCase = cases + i;
    const enum XML_Prop_Error expected
        = (testCase->valueValid ? XML_PROP_ERROR_NONE
                                : XML_PROP_ERROR_INVALID_VALUE);
    assert_true(XML_SetPropertyDouble(parser, testCase->key, testCase->value)
                == expected);
  }
#endif

  XML_ParserFree(parser);
}
END_TEST

START_TEST(test_props_maximum_tag_depth_default_disabled) {
  // Without opting in, the historical (unlimited) behavior must be kept.
  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);

  uint64_t limit = 123;
  assert_true(XML_GetPropertyUInt64(parser, XML_PROP_MAXIMUM_TAG_DEPTH,
                                    &limit)
              == XML_PROP_ERROR_NONE);
  assert_true(limit == 0);

  struct TagDepthCounters counters = {0, 0};
  XML_SetUserData(parser, &counters);
  XML_SetStartElementHandler(parser, tag_depth_start_handler);
  XML_SetEndElementHandler(parser, tag_depth_end_handler);

  char *const document = make_nested_document(50);
  assert_true(XML_Parse_SINGLE_BYTES(parser, document, (int)strlen(document),
                                     XML_TRUE)
              == XML_STATUS_OK);
  assert_true(counters.startElementEvents == 50);
  assert_true(counters.endElementEvents == 50);

  XML_ParserFree(parser);
}
END_TEST

START_TEST(test_props_maximum_tag_depth_boundary) {
  // A document whose depth is exactly the limit must parse, while a
  // document one level deeper must fail with a consistent error.
  const unsigned int depths[] = {1, 2, 3, 10};

  for (size_t i = 0; i < sizeof(depths) / sizeof(depths[0]); i++) {
    const unsigned int depth = depths[i];
    set_subtest("depth %u", depth);

    // Exactly at the limit: parses and reports every event.
    {
      XML_Parser parser = XML_ParserCreate(NULL);
      assert_true(parser != NULL);
      assert_true(XML_SetPropertyUInt64(parser, XML_PROP_MAXIMUM_TAG_DEPTH,
                                        depth)
                  == XML_PROP_ERROR_NONE);

      struct TagDepthCounters counters = {0, 0};
      XML_SetUserData(parser, &counters);
      XML_SetStartElementHandler(parser, tag_depth_start_handler);
      XML_SetEndElementHandler(parser, tag_depth_end_handler);

      char *const document = make_nested_document(depth);
      assert_true(XML_Parse_SINGLE_BYTES(parser, document,
                                         (int)strlen(document), XML_TRUE)
                  == XML_STATUS_OK);
      assert_true((unsigned int)counters.startElementEvents == depth);
      assert_true((unsigned int)counters.endElementEvents == depth);
      XML_ParserFree(parser);
    }

    // One level beyond the limit: fails, and the offending start tag is
    // never reported as an event.
    {
      XML_Parser parser = XML_ParserCreate(NULL);
      assert_true(parser != NULL);
      assert_true(XML_SetPropertyUInt64(parser, XML_PROP_MAXIMUM_TAG_DEPTH,
                                        depth)
                  == XML_PROP_ERROR_NONE);

      struct TagDepthCounters counters = {0, 0};
      XML_SetUserData(parser, &counters);
      XML_SetStartElementHandler(parser, tag_depth_start_handler);
      XML_SetEndElementHandler(parser, tag_depth_end_handler);

      char *const document = make_nested_document(depth + 1);
      assert_true(XML_Parse_SINGLE_BYTES(parser, document,
                                         (int)strlen(document), XML_TRUE)
                  == XML_STATUS_ERROR);
      assert_true(XML_GetErrorCode(parser)
                  == XML_ERROR_MAXIMUM_TAG_DEPTH_EXCEEDED);
      // Only the `depth` elements within the limit produced events.
      assert_true((unsigned int)counters.startElementEvents == depth);
      assert_true(counters.endElementEvents == 0);
      XML_ParserFree(parser);
    }
  }
}
END_TEST

START_TEST(test_props_maximum_tag_depth_empty_elements) {
  // Empty elements count as one level of nesting just like start/end
  // tag pairs.
  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);
  assert_true(XML_SetPropertyUInt64(parser, XML_PROP_MAXIMUM_TAG_DEPTH, 1)
              == XML_PROP_ERROR_NONE);

  struct TagDepthCounters counters = {0, 0};
  XML_SetUserData(parser, &counters);
  XML_SetStartElementHandler(parser, tag_depth_start_handler);
  XML_SetEndElementHandler(parser, tag_depth_end_handler);

  const char *const atLimit = "<a/>";
  assert_true(XML_Parse_SINGLE_BYTES(parser, atLimit,
                                     (int)strlen(atLimit), XML_TRUE)
              == XML_STATUS_OK);
  assert_true(counters.startElementEvents == 1);
  assert_true(counters.endElementEvents == 1);

  assert_true(XML_ParserReset(parser, NULL) == XML_TRUE);
  XML_SetPropertyUInt64(parser, XML_PROP_MAXIMUM_TAG_DEPTH, 1);
  XML_SetUserData(parser, &counters);
  XML_SetStartElementHandler(parser, tag_depth_start_handler);
  XML_SetEndElementHandler(parser, tag_depth_end_handler);
  counters.startElementEvents = 0;
  counters.endElementEvents = 0;

  const char *const beyondLimit = "<a><b/></a>";
  assert_true(XML_Parse_SINGLE_BYTES(parser, beyondLimit,
                                     (int)strlen(beyondLimit), XML_TRUE)
              == XML_STATUS_ERROR);
  assert_true(XML_GetErrorCode(parser)
              == XML_ERROR_MAXIMUM_TAG_DEPTH_EXCEEDED);
  // <a> produced events; the offending <b/> did not.
  assert_true(counters.startElementEvents == 1);
  assert_true(counters.endElementEvents == 0);

  XML_ParserFree(parser);
}
END_TEST

START_TEST(test_props_maximum_tag_depth_reset_clears_depth) {
  // After XML_ParserReset() the depth counter starts at zero again, so
  // a document at the limit must parse even if a previous document
  // exceeded it.
  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);
  assert_true(XML_SetPropertyUInt64(parser, XML_PROP_MAXIMUM_TAG_DEPTH, 2)
              == XML_PROP_ERROR_NONE);

  struct TagDepthCounters counters = {0, 0};
  XML_SetUserData(parser, &counters);
  XML_SetStartElementHandler(parser, tag_depth_start_handler);
  XML_SetEndElementHandler(parser, tag_depth_end_handler);

  char *const tooDeep = make_nested_document(3);
  assert_true(XML_Parse_SINGLE_BYTES(parser, tooDeep,
                                     (int)strlen(tooDeep), XML_TRUE)
              == XML_STATUS_ERROR);
  assert_true(XML_GetErrorCode(parser)
              == XML_ERROR_MAXIMUM_TAG_DEPTH_EXCEEDED);

  assert_true(XML_ParserReset(parser, NULL) == XML_TRUE);
  // The configured limit itself survives the reset.
  uint64_t limit = 0;
  assert_true(XML_GetPropertyUInt64(parser, XML_PROP_MAXIMUM_TAG_DEPTH,
                                    &limit)
              == XML_PROP_ERROR_NONE);
  assert_true(limit == 2);

  XML_SetUserData(parser, &counters);
  XML_SetStartElementHandler(parser, tag_depth_start_handler);
  XML_SetEndElementHandler(parser, tag_depth_end_handler);
  counters.startElementEvents = 0;
  counters.endElementEvents = 0;

  char *const atLimit = make_nested_document(2);
  assert_true(XML_Parse_SINGLE_BYTES(parser, atLimit,
                                     (int)strlen(atLimit), XML_TRUE)
              == XML_STATUS_OK);
  assert_true(counters.startElementEvents == 2);
  assert_true(counters.endElementEvents == 2);

  XML_ParserFree(parser);
}
END_TEST

START_TEST(test_props_maximum_tag_depth_relax_to_unlimited) {
  // Setting the limit back to 0 restores the historical behavior.
  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);
  assert_true(XML_SetPropertyUInt64(parser, XML_PROP_MAXIMUM_TAG_DEPTH, 1)
              == XML_PROP_ERROR_NONE);
  assert_true(XML_SetPropertyUInt64(parser, XML_PROP_MAXIMUM_TAG_DEPTH, 0)
              == XML_PROP_ERROR_NONE);

  uint64_t limit = 1;
  assert_true(XML_GetPropertyUInt64(parser, XML_PROP_MAXIMUM_TAG_DEPTH,
                                    &limit)
              == XML_PROP_ERROR_NONE);
  assert_true(limit == 0);

  char *const document = make_nested_document(20);
  assert_true(XML_Parse_SINGLE_BYTES(parser, document,
                                     (int)strlen(document), XML_TRUE)
              == XML_STATUS_OK);

  XML_ParserFree(parser);
}
END_TEST

START_TEST(test_props_maximum_tag_depth_subparser_inherits) {
  // The limit is document-wide: elements parsed by external entity
  // subparsers count towards the same depth as the parent document.
  const char *const document
      = "<!DOCTYPE a [<!ENTITY e SYSTEM 'entity.ent'>]><a>&e;</a>";

  // Parent <a> is at depth 1; entity content "<x><y/></x>" reaches
  // depth 3 in total.
  struct ExternalEntityDepthData passData = {
      "<x><y/></x>",
      XML_ERROR_NONE,
  };
  struct ExternalEntityDepthData failData = {
      "<x><y/></x>",
      XML_ERROR_MAXIMUM_TAG_DEPTH_EXCEEDED,
  };

  // Limit 3: the deepest element (y) is exactly at the limit.
  {
    XML_Parser parser = XML_ParserCreate(NULL);
    assert_true(parser != NULL);
    assert_true(XML_SetPropertyUInt64(parser, XML_PROP_MAXIMUM_TAG_DEPTH, 3)
                == XML_PROP_ERROR_NONE);
    XML_SetUserData(parser, &passData);
    XML_SetExternalEntityRefHandler(parser,
                                    tag_depth_external_entity_handler);
    assert_true(XML_Parse_SINGLE_BYTES(parser, document,
                                       (int)strlen(document), XML_TRUE)
                == XML_STATUS_OK);
    XML_ParserFree(parser);
  }

  // Limit 2: element y (depth 3) breaches the limit inside the
  // subparser, and the root parser reports the external entity failure.
  {
    XML_Parser parser = XML_ParserCreate(NULL);
    assert_true(parser != NULL);
    assert_true(XML_SetPropertyUInt64(parser, XML_PROP_MAXIMUM_TAG_DEPTH, 2)
                == XML_PROP_ERROR_NONE);
    XML_SetUserData(parser, &failData);
    XML_SetExternalEntityRefHandler(parser,
                                    tag_depth_external_entity_handler);
    assert_true(XML_Parse_SINGLE_BYTES(parser, document,
                                       (int)strlen(document), XML_TRUE)
                == XML_STATUS_ERROR);
    assert_true(XML_GetErrorCode(parser)
                == XML_ERROR_EXTERNAL_ENTITY_HANDLING);
    XML_ParserFree(parser);
  }
}
END_TEST

START_TEST(test_props_maximum_tag_depth_setter_errors) {
  // This test does not parse, so a single chunk-size run is enough.
  if (g_chunkSize != 0)
    return;

  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);

  // Wrong getter/setter types are rejected.
  assert_true(XML_SetPropertyBool(parser, XML_PROP_MAXIMUM_TAG_DEPTH,
                                  XML_TRUE)
              == XML_PROP_ERROR_INVALID_TYPE);
  assert_true(XML_SetPropertyDouble(parser, XML_PROP_MAXIMUM_TAG_DEPTH, 1.0)
              == XML_PROP_ERROR_INVALID_TYPE);
  XML_Bool dummyBool = XML_FALSE;
  double dummyDouble = 1.0;
  assert_true(XML_GetPropertyBool(parser, XML_PROP_MAXIMUM_TAG_DEPTH,
                                  &dummyBool)
              == XML_PROP_ERROR_INVALID_TYPE);
  assert_true(XML_GetPropertyDouble(parser, XML_PROP_MAXIMUM_TAG_DEPTH,
                                    &dummyDouble)
              == XML_PROP_ERROR_INVALID_TYPE);
  assert_true(XML_GetPropertyUInt64(parser, XML_PROP_MAXIMUM_TAG_DEPTH,
                                    NULL)
              == XML_PROP_ERROR_INVALID_VALUE);

  // The limit can only be configured on the root parser.
  XML_Parser subParser = XML_ExternalEntityParserCreate(parser, NULL, NULL);
#if defined(XML_DTD)
  assert_true(subParser != NULL);
  assert_true(XML_SetPropertyUInt64(subParser, XML_PROP_MAXIMUM_TAG_DEPTH, 5)
              == XML_PROP_ERROR_PARSER_NOT_ROOT);
  // The subparser still sees the root parser's limit.
  uint64_t limit = 0;
  assert_true(XML_GetPropertyUInt64(subParser, XML_PROP_MAXIMUM_TAG_DEPTH,
                                    &limit)
              == XML_PROP_ERROR_NONE);
  assert_true(limit == 0);
  XML_ParserFree(subParser);
#else
  assert_true(subParser == NULL);
#endif

  XML_ParserFree(parser);
}
END_TEST

void
make_props_test_case(Suite *s) {
  TCase *const tc_props = tcase_create("properties tests");
  suite_add_tcase(s, tc_props);

  tcase_add_test(tc_props, test_props_getter_defaults);
  tcase_add_test(tc_props, test_props_getter_error_parser_null);
  tcase_add_test(tc_props, test_props_getter_error_invalid_key);
  tcase_add_test(tc_props, test_props_getter_error_invalid_type);
  tcase_add_test(tc_props, test_props_getter_error_invalid_value);

  tcase_add_test(tc_props, test_props_setter_effective);
  tcase_add_test(tc_props, test_props_setter_error_parser_null);
  tcase_add_test(tc_props, test_props_setter_error_parser_not_root);
  tcase_add_test(tc_props, test_props_setter_error_parser_started);
  tcase_add_test(tc_props, test_props_setter_error_invalid_key);
  tcase_add_test(tc_props, test_props_setter_error_invalid_type);
  tcase_add_test(tc_props, test_props_setter_error_invalid_value);

  tcase_add_test(tc_props, test_props_maximum_tag_depth_default_disabled);
  tcase_add_test(tc_props, test_props_maximum_tag_depth_boundary);
  tcase_add_test(tc_props, test_props_maximum_tag_depth_empty_elements);
  tcase_add_test(tc_props, test_props_maximum_tag_depth_reset_clears_depth);
  tcase_add_test(tc_props, test_props_maximum_tag_depth_relax_to_unlimited);
  tcase_add_test__ifdef_xml_dtd(tc_props,
                                test_props_maximum_tag_depth_subparser_inherits);
  tcase_add_test(tc_props, test_props_maximum_tag_depth_setter_errors);
}
