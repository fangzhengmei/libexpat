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
#include <string.h>

static int XMLCALL
security_config_external_entity_handler(XML_Parser parser,
                                        const XML_Char *context,
                                        const XML_Char *base,
                                        const XML_Char *systemId,
                                        const XML_Char *publicId) {
  int *const callCount = (int *)XML_GetUserData(parser);
  UNUSED_P(context);
  UNUSED_P(base);
  UNUSED_P(systemId);
  UNUSED_P(publicId);
  if (callCount != NULL)
    (*callCount)++;
  return XML_STATUS_OK;
}

static XML_Security_Configuration
make_security_configuration(void) {
  XML_Security_Configuration configuration;
  memset(&configuration, 0, sizeof(configuration));
#ifdef XML_DTD
  configuration.allowDtd = XML_TRUE;
#else
  configuration.allowDtd = XML_FALSE;
#endif
  configuration.allowExternalGeneralEntities = XML_FALSE;
  configuration.allowExternalParameterEntities = XML_FALSE;
  configuration.reparseDeferralEnabled = g_reparseDeferralEnabledDefault;
  configuration.externalEntityRefHandler = NULL;
#if XML_GE == 1
  configuration.resourceLimits.billionLaughsMaximumAmplification
      = EXPAT_BILLION_LAUGHS_ATTACK_PROTECTION_MAXIMUM_AMPLIFICATION_DEFAULT;
  configuration.resourceLimits.billionLaughsActivationThresholdBytes
      = EXPAT_BILLION_LAUGHS_ATTACK_PROTECTION_ACTIVATION_THRESHOLD_DEFAULT;
  configuration.resourceLimits.allocationMaximumAmplification
      = EXPAT_ALLOC_TRACKER_MAXIMUM_AMPLIFICATION_DEFAULT;
  configuration.resourceLimits.allocationActivationThresholdBytes
      = EXPAT_ALLOC_TRACKER_ACTIVATION_THRESHOLD_DEFAULT;
#endif
  return configuration;
}

static void XMLCALL
security_config_suspend_handler(void *userData, const XML_Char *name,
                                const XML_Char **atts) {
  XML_Parser parser = (XML_Parser)userData;
  UNUSED_P(name);
  UNUSED_P(atts);
  XML_StopParser(parser, XML_TRUE);
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

START_TEST(test_security_configuration_check_does_not_modify_parser) {
  if (g_chunkSize != 0)
    return;

  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);
  int handlerCallCount = 0;
  XML_SetUserData(parser, &handlerCallCount);

  XML_Bool reparseDeferralEnabled = XML_FALSE;
  assert_true(XML_GetPropertyBool(parser, XML_PROP_REPARSE_DEFERRAL_ENABLED,
                                  &reparseDeferralEnabled)
              == XML_PROP_ERROR_NONE);
  assert_true(reparseDeferralEnabled == g_reparseDeferralEnabledDefault);

  XML_Security_Configuration validConfiguration
      = make_security_configuration();
  validConfiguration.reparseDeferralEnabled = XML_FALSE;
#if XML_GE == 1 || defined(XML_DTD)
  validConfiguration.externalEntityRefHandler
      = security_config_external_entity_handler;
#endif
#ifdef XML_DTD
  validConfiguration.allowExternalParameterEntities = XML_TRUE;
#endif
#if XML_GE == 1
  validConfiguration.allowExternalGeneralEntities = XML_TRUE;
  validConfiguration.resourceLimits.billionLaughsMaximumAmplification = 2.5f;
  validConfiguration.resourceLimits.billionLaughsActivationThresholdBytes = 25;
  validConfiguration.resourceLimits.allocationMaximumAmplification = 3.5f;
  validConfiguration.resourceLimits.allocationActivationThresholdBytes = 35;
#endif
  assert_true(XML_CheckSecurityConfiguration(parser, &validConfiguration)
              == XML_PROP_ERROR_NONE);

  assert_true(XML_GetPropertyBool(parser, XML_PROP_REPARSE_DEFERRAL_ENABLED,
                                  &reparseDeferralEnabled)
              == XML_PROP_ERROR_NONE);
  assert_true(reparseDeferralEnabled == g_reparseDeferralEnabledDefault);

  assert_true(XML_ApplySecurityConfiguration(parser, &validConfiguration)
              == XML_PROP_ERROR_NONE);

  assert_true(XML_GetPropertyBool(parser, XML_PROP_REPARSE_DEFERRAL_ENABLED,
                                  &reparseDeferralEnabled)
              == XML_PROP_ERROR_NONE);
  assert_true(reparseDeferralEnabled == XML_FALSE);
  assert_true(handlerCallCount == 0);

  XML_Security_Configuration invalidConfiguration
      = make_security_configuration();
  invalidConfiguration.reparseDeferralEnabled
      = (XML_Bool)42; // intentionally invalid
  assert_true(XML_ApplySecurityConfiguration(parser, &invalidConfiguration)
              == XML_PROP_ERROR_INVALID_VALUE);

  assert_true(reparseDeferralEnabled == XML_FALSE);

#if XML_GE == 1
  double maximumAmplification = 0.0;
  uint64_t activationThresholdBytes = 0;
  assert_true(XML_GetPropertyDouble(
                  parser, XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION,
                  &maximumAmplification)
              == XML_PROP_ERROR_NONE);
  assert_true(maximumAmplification == 2.5);
  assert_true(XML_GetPropertyUInt64(
                  parser, XML_PROP_BILLION_LAUGHS_ACTIVATION_THRESHOLD,
                  &activationThresholdBytes)
              == XML_PROP_ERROR_NONE);
  assert_true(activationThresholdBytes == 25);
#endif

  assert_true(XML_GetPropertyBool(parser, XML_PROP_REPARSE_DEFERRAL_ENABLED,
                                  &reparseDeferralEnabled)
              == XML_PROP_ERROR_NONE);
  assert_true(reparseDeferralEnabled == XML_FALSE);

  XML_ParserFree(parser);
}
END_TEST

START_TEST(test_security_configuration_invalid_combinations) {
  if (g_chunkSize != 0)
    return;

  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);

  assert_true(XML_CheckSecurityConfiguration(NULL, NULL)
              == XML_PROP_ERROR_PARSER_NULL);
  assert_true(XML_CheckSecurityConfiguration(parser, NULL)
              == XML_PROP_ERROR_CONFIG_NULL);

  XML_Security_Configuration configuration = make_security_configuration();
  configuration.reparseDeferralEnabled = (XML_Bool)42;
  assert_true(XML_CheckSecurityConfiguration(parser, &configuration)
              == XML_PROP_ERROR_INVALID_VALUE);
  configuration = make_security_configuration();

#ifdef XML_DTD
  configuration.allowExternalParameterEntities = XML_TRUE;
  assert_true(XML_CheckSecurityConfiguration(parser, &configuration)
              == XML_PROP_ERROR_INCOMPATIBLE_CONFIGURATION);
  configuration = make_security_configuration();
#else
  configuration.allowDtd = XML_TRUE;
  assert_true(XML_CheckSecurityConfiguration(parser, &configuration)
              == XML_PROP_ERROR_DTD_UNAVAILABLE);
  configuration = make_security_configuration();

  configuration.allowExternalParameterEntities = XML_TRUE;
  assert_true(XML_CheckSecurityConfiguration(parser, &configuration)
              == XML_PROP_ERROR_DTD_UNAVAILABLE);
  configuration = make_security_configuration();
#endif

#if XML_GE == 1
  configuration.allowExternalGeneralEntities = XML_TRUE;
  assert_true(XML_CheckSecurityConfiguration(parser, &configuration)
              == XML_PROP_ERROR_INCOMPATIBLE_CONFIGURATION);
  configuration = make_security_configuration();

  configuration.resourceLimits.billionLaughsMaximumAmplification = NAN;
  assert_true(XML_CheckSecurityConfiguration(parser, &configuration)
              == XML_PROP_ERROR_INVALID_VALUE);
  configuration = make_security_configuration();

  configuration.resourceLimits.allocationMaximumAmplification = 0.5f;
  assert_true(XML_CheckSecurityConfiguration(parser, &configuration)
              == XML_PROP_ERROR_INVALID_VALUE);
  configuration = make_security_configuration();
#else
  configuration.allowExternalGeneralEntities = XML_TRUE;
  assert_true(XML_CheckSecurityConfiguration(parser, &configuration)
              == XML_PROP_ERROR_GENERAL_ENTITIES_UNAVAILABLE);
  configuration = make_security_configuration();

  configuration.externalEntityRefHandler
      = security_config_external_entity_handler;
  assert_true(XML_CheckSecurityConfiguration(parser, &configuration)
              == XML_PROP_ERROR_EXTERNAL_ENTITIES_UNAVAILABLE);
  configuration = make_security_configuration();

  configuration.resourceLimits.billionLaughsMaximumAmplification = 1.0f;
  assert_true(XML_CheckSecurityConfiguration(parser, &configuration)
              == XML_PROP_ERROR_RESOURCE_LIMITS_UNAVAILABLE);
  configuration = make_security_configuration();
#endif

  XML_Parser subParser = XML_ExternalEntityParserCreate(parser, NULL, NULL);
#ifdef XML_DTD
  assert_true(subParser != NULL);
  assert_true(XML_CheckSecurityConfiguration(subParser, &configuration)
              == XML_PROP_ERROR_PARSER_NOT_ROOT);
  XML_ParserFree(subParser);
#else
  assert_true(subParser == NULL);
#endif

  assert_true(XML_Parse_SINGLE_BYTES(parser, "", 0, XML_FALSE)
              == XML_STATUS_OK);
  assert_true(XML_CheckSecurityConfiguration(parser, &configuration)
              == XML_PROP_ERROR_PARSER_BUSY);
  assert_true(XML_ApplySecurityConfiguration(parser, &configuration)
              == XML_PROP_ERROR_PARSER_BUSY);

  XML_ParserFree(parser);
}
END_TEST

START_TEST(test_security_configuration_applies_and_enforces_boundaries) {
  if (g_chunkSize != 0)
    return;

  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);

  XML_Security_Configuration configuration
      = make_security_configuration();
  configuration.allowDtd = XML_FALSE;
  configuration.reparseDeferralEnabled = XML_FALSE;
#if XML_GE == 1
  configuration.resourceLimits.billionLaughsMaximumAmplification = 1.0f;
  configuration.resourceLimits.billionLaughsActivationThresholdBytes = 0;
#endif

  assert_true(XML_ApplySecurityConfiguration(parser, &configuration)
              == XML_PROP_ERROR_NONE);

  XML_Bool reparseDeferralEnabled = XML_TRUE;
  assert_true(XML_GetPropertyBool(parser, XML_PROP_REPARSE_DEFERRAL_ENABLED,
                                  &reparseDeferralEnabled)
              == XML_PROP_ERROR_NONE);
  assert_true(reparseDeferralEnabled == XML_FALSE);

  const char *const dtdDocument
      = "<!DOCTYPE root><root/>";
#ifdef XML_DTD
  assert_true(XML_UseForeignDTD(parser, XML_TRUE)
              == XML_ERROR_FEATURE_REQUIRES_XML_DTD);
  assert_true(XML_Parse_SINGLE_BYTES(parser, dtdDocument,
                                     (int)strlen(dtdDocument), XML_TRUE)
              == XML_STATUS_ERROR);
  assert_true(XML_GetErrorCode(parser)
              == XML_ERROR_FEATURE_REQUIRES_XML_DTD);
#else
  assert_true(XML_Parse_SINGLE_BYTES(parser, dtdDocument,
                                     (int)strlen(dtdDocument), XML_TRUE)
              == XML_STATUS_OK);
#endif

  XML_ParserFree(parser);

#if XML_GE == 1
  parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);
#ifdef XML_DTD
  configuration.allowDtd = XML_TRUE;
#endif
  assert_true(XML_ApplySecurityConfiguration(parser, &configuration)
              == XML_PROP_ERROR_NONE);

  const char *const externalEntityDocument
      = "<!DOCTYPE root [<!ENTITY external SYSTEM 'external.txt'>]>"
        "<root>&external;</root>";
  assert_true(XML_Parse_SINGLE_BYTES(parser, externalEntityDocument,
                                     (int)strlen(externalEntityDocument),
                                     XML_TRUE)
              == XML_STATUS_OK);

  XML_ParserFree(parser);
#endif
}
END_TEST

#if XML_GE == 1
START_TEST(test_security_configuration_limit_survives_suspend_resume) {
  if (g_chunkSize != 0)
    return;

  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);

  XML_Security_Configuration configuration
      = make_security_configuration();
  configuration.resourceLimits.billionLaughsMaximumAmplification = 1.0f;
  configuration.resourceLimits.billionLaughsActivationThresholdBytes = 0;
  assert_true(XML_ApplySecurityConfiguration(parser, &configuration)
              == XML_PROP_ERROR_NONE);

  XML_SetUserData(parser, parser);
  XML_SetStartElementHandler(parser, security_config_suspend_handler);

  const char *const document
      = "<!DOCTYPE root [<!ENTITY text 'expanded text'>]>"
        "<suspend>&text;</suspend>";
  enum XML_Status status
      = XML_Parse(parser, document, (int)strlen(document), XML_FALSE);
  assert_true(status == XML_STATUS_SUSPENDED);
  assert_true(XML_ResumeParser(parser) == XML_STATUS_ERROR);
  assert_true(XML_GetErrorCode(parser)
              == XML_ERROR_AMPLIFICATION_LIMIT_BREACH);

  XML_ParserFree(parser);
}
END_TEST
#endif

START_TEST(test_security_configuration_inherits_to_subparser) {
  if (g_chunkSize != 0)
    return;

  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);

  XML_Security_Configuration configuration
      = make_security_configuration();
  configuration.reparseDeferralEnabled = XML_FALSE;
#if XML_GE == 1
  configuration.resourceLimits.billionLaughsMaximumAmplification = 1.0f;
  configuration.resourceLimits.billionLaughsActivationThresholdBytes = 0;
  double maximumAmplification = 0.0;
  uint64_t activationThresholdBytes = 1;
#endif

  assert_true(XML_ApplySecurityConfiguration(parser, &configuration)
              == XML_PROP_ERROR_NONE);

#if XML_GE == 1
  assert_true(XML_GetPropertyDouble(
                  parser, XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION,
                  &maximumAmplification)
              == XML_PROP_ERROR_NONE);
  assert_true(maximumAmplification == 1.0);
  assert_true(XML_GetPropertyUInt64(
                  parser, XML_PROP_BILLION_LAUGHS_ACTIVATION_THRESHOLD,
                  &activationThresholdBytes)
              == XML_PROP_ERROR_NONE);
  assert_true(activationThresholdBytes == 0);
#endif

  XML_Parser subParser = XML_ExternalEntityParserCreate(parser, "e1",
                                                        NULL);
#if XML_GE == 1
  assert_true(subParser != NULL);
  XML_Bool reparseDeferralEnabled = XML_TRUE;
  assert_true(XML_GetPropertyBool(subParser,
                                  XML_PROP_REPARSE_DEFERRAL_ENABLED,
                                  &reparseDeferralEnabled)
              == XML_PROP_ERROR_NONE);
  assert_true(reparseDeferralEnabled == XML_FALSE);

  const char *const externalDocument = "<external/>";
  assert_true(XML_Parse_SINGLE_BYTES(subParser, externalDocument,
                                     (int)strlen(externalDocument), XML_TRUE)
              == XML_STATUS_ERROR);
  assert_true(XML_GetErrorCode(subParser)
              == XML_ERROR_AMPLIFICATION_LIMIT_BREACH);

  XML_ParserFree(subParser);
#else
  if (subParser != NULL)
    XML_ParserFree(subParser);
#endif

  XML_ParserFree(parser);
}
END_TEST

START_TEST(test_legacy_property_setters_remain_independent) {
  if (g_chunkSize != 0)
    return;

  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);

  assert_true(XML_SetReparseDeferralEnabled(parser, XML_FALSE) == XML_TRUE);
  XML_Bool reparseDeferralEnabled = XML_TRUE;
  assert_true(XML_GetPropertyBool(parser, XML_PROP_REPARSE_DEFERRAL_ENABLED,
                                  &reparseDeferralEnabled)
              == XML_PROP_ERROR_NONE);
  assert_true(reparseDeferralEnabled == XML_FALSE);

#if XML_GE == 1
  assert_true(XML_SetBillionLaughsAttackProtectionMaximumAmplification(
                  parser, 2.0f)
              == XML_TRUE);
  double maximumAmplification = 0.0;
  assert_true(XML_GetPropertyDouble(
                  parser, XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION,
                  &maximumAmplification)
              == XML_PROP_ERROR_NONE);
  assert_true(maximumAmplification == 2.0);
#endif

  XML_ParserFree(parser);
}
END_TEST

#if defined(XML_DTD) && XML_GE == 1
START_TEST(test_security_configuration_enables_external_entities) {
  if (g_chunkSize != 0)
    return;

  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);

  int handlerCallCount = 0;
  XML_Security_Configuration configuration
      = make_security_configuration();
  configuration.allowExternalParameterEntities = XML_TRUE;
  configuration.allowExternalGeneralEntities = XML_TRUE;
  configuration.externalEntityRefHandler
      = security_config_external_entity_handler;
  XML_SetUserData(parser, &handlerCallCount);

  assert_true(XML_ApplySecurityConfiguration(parser, &configuration)
              == XML_PROP_ERROR_NONE);

  const char *const document
      = "<!DOCTYPE root SYSTEM 'external.dtd'>"
        "<root>&external;</root>";
  assert_true(XML_Parse_SINGLE_BYTES(parser, document, (int)strlen(document),
                                     XML_TRUE)
              == XML_STATUS_OK);
  assert_true(handlerCallCount == 1);

  XML_ParserFree(parser);
}
END_TEST
#endif

#if XML_GE == 1
START_TEST(test_security_configuration_enables_general_entities_without_dtd) {
  if (g_chunkSize != 0)
    return;

  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);

  int handlerCallCount = 0;
  XML_Security_Configuration configuration
      = make_security_configuration();
#ifndef XML_DTD
  configuration.allowDtd = XML_FALSE;
#endif
  configuration.allowExternalGeneralEntities = XML_TRUE;
  configuration.externalEntityRefHandler
      = security_config_external_entity_handler;
  XML_SetUserData(parser, &handlerCallCount);

  assert_true(XML_ApplySecurityConfiguration(parser, &configuration)
              == XML_PROP_ERROR_NONE);

  const char *const document
      = "<!DOCTYPE root [<!ENTITY external SYSTEM 'external.txt'>]>"
        "<root>&external;</root>";
  assert_true(XML_Parse_SINGLE_BYTES(parser, document, (int)strlen(document),
                                     XML_TRUE)
              == XML_STATUS_OK);
  assert_true(handlerCallCount == 1);

  XML_ParserFree(parser);
}
END_TEST
#endif

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
  tcase_add_test(tc_props,
                 test_security_configuration_check_does_not_modify_parser);
  tcase_add_test(tc_props,
                 test_security_configuration_invalid_combinations);
  tcase_add_test(tc_props,
                 test_security_configuration_applies_and_enforces_boundaries);
#if XML_GE == 1
  tcase_add_test(tc_props,
                 test_security_configuration_limit_survives_suspend_resume);
#endif
  tcase_add_test(tc_props,
                 test_security_configuration_inherits_to_subparser);
  tcase_add_test(tc_props,
                 test_legacy_property_setters_remain_independent);
#if defined(XML_DTD) && XML_GE == 1
  tcase_add_test(tc_props,
                 test_security_configuration_enables_external_entities);
#endif
#if XML_GE == 1
  tcase_add_test(tc_props,
                 test_security_configuration_enables_general_entities_without_dtd);
#endif
}
