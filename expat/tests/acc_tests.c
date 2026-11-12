/* Tests in the "accounting" test case for the Expat test suite
                            __  __            _
                         ___\ \/ /_ __   __ _| |_
                        / _ \\  /| '_ \ / _` | __|
                       |  __//  \| |_) | (_| | |_
                        \___/_/\_\ .__/ \__,_|\__|
                                 |_| XML parser

   Copyright (c) 2001-2006 Fred L. Drake, Jr. <fdrake@users.sourceforge.net>
   Copyright (c) 2003      Greg Stein <gstein@users.sourceforge.net>
   Copyright (c) 2005-2007 Steven Solie <steven@solie.ca>
   Copyright (c) 2005-2012 Karl Waclawek <karl@waclawek.net>
   Copyright (c) 2016-2026 Sebastian Pipping <sebastian@pipping.org>
   Copyright (c) 2017-2022 Rhodri James <rhodri@wildebeest.org.uk>
   Copyright (c) 2017      Joe Orton <jorton@redhat.com>
   Copyright (c) 2017      José Gutiérrez de la Concha <jose@zeroc.com>
   Copyright (c) 2018      Marco Maggi <marco.maggi-ipsu@poste.it>
   Copyright (c) 2019      David Loffredo <loffredo@steptools.com>
   Copyright (c) 2020      Tim Gates <tim.gates@iress.com>
   Copyright (c) 2021      Donghee Na <donghee.na@python.org>
   Copyright (c) 2023      Sony Corporation / Snild Dolkow <snild@sony.com>
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

#include <math.h> /* NAN, INFINITY */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "expat.h"
#include "internal.h"
#include "common.h"
#include "minicheck.h"
#include "chardata.h"
#include "handlers.h"
#include "xcs.h"
#include "acc_tests.h"

#if XML_GE == 1
struct ExternalBudgetData {
  int handlerCalls;
  int childCreations;
};

static int XMLCALL
external_budget_handler(XML_Parser parser, const XML_Char *context,
                        const XML_Char *base, const XML_Char *systemId,
                        const XML_Char *publicId) {
  struct ExternalBudgetData *const data = XML_GetUserData(parser);
  const char *text;

  UNUSED_P(base);
  UNUSED_P(publicId);

  data->handlerCalls++;
  if (xcscmp(systemId, XCS("outer.ent")) == 0) {
    text = "<!DOCTYPE r [<!ENTITY inner SYSTEM 'inner.ent'>]>"
           "<r>&inner;</r>";
  } else if (xcscmp(systemId, XCS("inner.ent")) == 0) {
    text = "<!ENTITY e 'value'><r>&e;</r>";
  } else {
    text = "<!ENTITY e 'value'><r>&e;</r>";
  }

  XML_Parser child = XML_ExternalEntityParserCreate(parser, context, NULL);
  if (child == NULL)
    return XML_STATUS_ERROR;

  data->childCreations++;
  const enum XML_Status status
      = XML_Parse_SINGLE_BYTES(child, text, (int)strlen(text), XML_TRUE);
  XML_ParserFree(child);
  return status;
}

static XML_Parser
create_budget_parser(struct ExternalBudgetData *data, XML_Bool shared,
                     XML_Bool useAllocTracker) {
  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);

  XML_Bool sharedValue = XML_FALSE;
  assert_true(XML_GetPropertyBool(
                  parser, XML_PROP_EXTERNAL_ENTITY_RESOURCE_BUDGET_SHARED,
                  &sharedValue)
              == XML_PROP_ERROR_NONE);
  assert_true(sharedValue == XML_FALSE);

  XML_SetUserData(parser, data);
  XML_SetExternalEntityRefHandler(parser, external_budget_handler);
  assert_true(XML_SetPropertyBool(
                  parser, XML_PROP_EXTERNAL_ENTITY_RESOURCE_BUDGET_SHARED,
                  shared)
              == XML_PROP_ERROR_NONE);
  assert_true(XML_GetPropertyBool(
                  parser, XML_PROP_EXTERNAL_ENTITY_RESOURCE_BUDGET_SHARED,
                  &sharedValue)
              == XML_PROP_ERROR_NONE);
  assert_true(sharedValue == shared);

  if (useAllocTracker) {
    assert_true(XML_SetAllocTrackerMaximumAmplification(parser, 1.0f)
                == XML_TRUE);
    assert_true(XML_SetAllocTrackerActivationThreshold(parser, 0)
                == XML_TRUE);
  } else {
    assert_true(XML_SetBillionLaughsAttackProtectionMaximumAmplification(
                    parser, 1.0f)
                == XML_TRUE);
    assert_true(XML_SetBillionLaughsAttackProtectionActivationThreshold(
                    parser, 0)
                == XML_TRUE);
  }
  return parser;
}

START_TEST(test_external_entity_resource_budget_property) {
  XML_Parser parser = XML_ParserCreate(NULL);
  XML_Parser child;
  XML_Bool value = XML_TRUE;

  assert_true(parser != NULL);
  assert_true(XML_SetPropertyBool(
                  parser, XML_PROP_EXTERNAL_ENTITY_RESOURCE_BUDGET_SHARED,
                  42)
              == XML_PROP_ERROR_INVALID_VALUE);
  assert_true(XML_SetPropertyBool(
                  parser, XML_PROP_EXTERNAL_ENTITY_RESOURCE_BUDGET_SHARED,
                  XML_TRUE)
              == XML_PROP_ERROR_NONE);

  child = XML_ExternalEntityParserCreate(parser, XCS("entity"), NULL);
  assert_true(child != NULL);
  assert_true(XML_SetPropertyBool(
                  child, XML_PROP_EXTERNAL_ENTITY_RESOURCE_BUDGET_SHARED,
                  XML_TRUE)
              == XML_PROP_ERROR_PARSER_NOT_ROOT);
  XML_ParserFree(child);
  XML_ParserFree(parser);

  assert_true(XML_SetPropertyBool(
                  NULL, XML_PROP_EXTERNAL_ENTITY_RESOURCE_BUDGET_SHARED,
                  XML_TRUE)
              == XML_PROP_ERROR_PARSER_NULL);
  assert_true(XML_GetPropertyBool(
                  NULL, XML_PROP_EXTERNAL_ENTITY_RESOURCE_BUDGET_SHARED,
                  &value)
              == XML_PROP_ERROR_PARSER_NULL);

  parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);
  assert_true(XML_SetPropertyBool(
                  parser, XML_PROP_EXTERNAL_ENTITY_RESOURCE_BUDGET_SHARED,
                  XML_TRUE)
              == XML_PROP_ERROR_NONE);
  assert_true(XML_ParserReset(parser, NULL) == XML_TRUE);
  value = XML_TRUE;
  assert_true(XML_GetPropertyBool(
                  parser, XML_PROP_EXTERNAL_ENTITY_RESOURCE_BUDGET_SHARED,
                  &value)
              == XML_PROP_ERROR_NONE);
  assert_true(value == XML_FALSE);
  XML_ParserFree(parser);
}
END_TEST

START_TEST(test_external_entity_resource_budget_nested_entities) {
  if (g_chunkSize != 0 || g_reparseDeferralEnabledDefault)
    return;

  const char doc[] = "<!DOCTYPE r [<!ENTITY outer SYSTEM 'outer.ent'>]>"
                     "<r>&outer;</r>";
  struct ExternalBudgetData data;

  for (int shared = 0; shared <= 1; shared++) {
    data.handlerCalls = 0;
    data.childCreations = 0;
    XML_Parser parser = create_budget_parser(
        &data, shared ? XML_TRUE : XML_FALSE, XML_FALSE);
    const enum XML_Status status
        = XML_Parse_SINGLE_BYTES(parser, doc, (int)sizeof(doc) - 1,
                                 XML_TRUE);

    assert_true(status == XML_STATUS_ERROR);
    assert_true(data.handlerCalls >= 1);
    assert_true(data.childCreations == data.handlerCalls);
    XML_ParserFree(parser);
  }
}
END_TEST

START_TEST(test_external_entity_resource_budget_creation_failure) {
  if (g_chunkSize != 0 || g_reparseDeferralEnabledDefault)
    return;

  const char doc[] = "<!DOCTYPE r [<!ENTITY ext SYSTEM 'entity.ent'>]>"
                     "<r>&ext;</r>";
  struct ExternalBudgetData data;

  for (int shared = 0; shared <= 1; shared++) {
    data.handlerCalls = 0;
    data.childCreations = 0;
    XML_Parser parser = create_budget_parser(
        &data, shared ? XML_TRUE : XML_FALSE, XML_TRUE);
    const enum XML_Status status
        = XML_Parse_SINGLE_BYTES(parser, doc, (int)sizeof(doc) - 1,
                                 XML_TRUE);

    if (status == XML_STATUS_ERROR) {
      assert_true(data.handlerCalls >= 0);
    } else {
      assert_true(status == XML_STATUS_OK);
    }
    assert_true(data.childCreations == data.handlerCalls);

    XML_ParsingStatus parsingStatus;
    XML_GetParsingStatus(parser, &parsingStatus);
    XML_ParserFree(parser);
  }
}
END_TEST

START_TEST(test_accounting_precision) {
  struct AccountingTestCase cases[] = {
      {"<e/>", NULL, NULL, 0},
      {"<e></e>", NULL, NULL, 0},

      /* Attributes */
      {"<e k1=\"v2\" k2=\"v2\"/>", NULL, NULL, 0},
      {"<e k1=\"v2\" k2=\"v2\"></e>", NULL, NULL, 0},
      {"<p:e xmlns:p=\"https://domain.invalid/\" />", NULL, NULL, 0},
      {"<e k=\"&amp;&apos;&gt;&lt;&quot;\" />", NULL, NULL,
       sizeof(XML_Char) * 5 /* number of predefined entities */},
      {"<e1 xmlns='https://example.org/'>\n"
       "  <e2 xmlns=''/>\n"
       "</e1>",
       NULL, NULL, 0},

      /* Text */
      {"<e>text</e>", NULL, NULL, 0},
      {"<e1><e2>text1<e3/>text2</e2></e1>", NULL, NULL, 0},
      {"<e>&amp;&apos;&gt;&lt;&quot;</e>", NULL, NULL,
       sizeof(XML_Char) * 5 /* number of predefined entities */},
      {"<e>&#65;&#41;</e>", NULL, NULL, 0},

      /* Prolog */
      {"<?xml version=\"1.0\"?><root/>", NULL, NULL, 0},

      /* Whitespace */
      {"  <e1>  <e2>  </e2>  </e1>  ", NULL, NULL, 0},
      {"<e1  ><e2  /></e1  >", NULL, NULL, 0},
      {"<e1><e2 k = \"v\"/><e3 k = 'v'/></e1>", NULL, NULL, 0},

      /* Comments */
      {"<!-- Comment --><e><!-- Comment --></e>", NULL, NULL, 0},

      /* Processing instructions */
      {"<?xml-stylesheet type=\"text/xsl\" href=\"https://domain.invalid/\" media=\"all\"?><e/>",
       NULL, NULL, 0},
      {"<?pi0?><?pi1 ?><?pi2  ?><r/><?pi4?>", NULL, NULL, 0},
#  ifdef XML_DTD
      {"<?pi0?><?pi1 ?><?pi2  ?><!DOCTYPE r SYSTEM 'first.ent'><r/>",
       "<?pi3?><!ENTITY % e1 SYSTEM 'second.ent'><?pi4?>%e1;<?pi5?>", "<?pi6?>",
       0},
#  endif /* XML_DTD */

      /* CDATA */
      {"<e><![CDATA[one two three]]></e>", NULL, NULL, 0},
      /* The following is the essence of this OSS-Fuzz finding:
         https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=34302
         https://oss-fuzz.com/testcase-detail/4860575394955264
      */
      {"<!DOCTYPE r [\n"
       "<!ENTITY e \"111<![CDATA[2 <= 2]]>333\">\n"
       "]>\n"
       "<r>&e;</r>\n",
       NULL, NULL, sizeof(XML_Char) * strlen("111<![CDATA[2 <= 2]]>333")},

#  ifdef XML_DTD
      /* Conditional sections */
      {"<!DOCTYPE r [\n"
       "<!ENTITY % draft 'INCLUDE'>\n"
       "<!ENTITY % final 'IGNORE'>\n"
       "<!ENTITY % import SYSTEM \"first.ent\">\n"
       "%import;\n"
       "]>\n"
       "<r/>\n",
       "<![%draft;[<!--1-->]]>\n"
       "<![%final;[<!--22-->]]>",
       NULL, sizeof(XML_Char) * (strlen("INCLUDE") + strlen("IGNORE"))},
#  endif /* XML_DTD */

      /* General entities */
      {"<!DOCTYPE root [\n"
       "<!ENTITY nine \"123456789\">\n"
       "]>\n"
       "<root>&nine;</root>",
       NULL, NULL, sizeof(XML_Char) * strlen("123456789")},
      {"<!DOCTYPE root [\n"
       "<!ENTITY nine \"123456789\">\n"
       "]>\n"
       "<root k1=\"&nine;\"/>",
       NULL, NULL, sizeof(XML_Char) * strlen("123456789")},
      {"<!DOCTYPE root [\n"
       "<!ENTITY nine \"123456789\">\n"
       "<!ENTITY nine2 \"&nine;&nine;\">\n"
       "]>\n"
       "<root>&nine2;&nine2;&nine2;</root>",
       NULL, NULL,
       sizeof(XML_Char) * 3 /* calls to &nine2; */ * 2 /* calls to &nine; */
           * (strlen("&nine;") + strlen("123456789"))},
      {"<!DOCTYPE r [\n"
       "  <!ENTITY five SYSTEM 'first.ent'>\n"
       "]>\n"
       "<r>&five;</r>",
       "12345", NULL, 0},
      {"<!DOCTYPE r [\n"
       "  <!ENTITY five SYSTEM 'first.ent'>\n"
       "]>\n"
       "<r>&five;</r>",
       "\xEF\xBB\xBF" /* UTF-8 BOM */, NULL, 0},

#  ifdef XML_DTD
      /* Parameter entities */
      {"<!DOCTYPE r [\n"
       "<!ENTITY % comment \"<!---->\">\n"
       "%comment;\n"
       "]>\n"
       "<r/>",
       NULL, NULL, sizeof(XML_Char) * strlen("<!---->")},
      {"<!DOCTYPE r [\n"
       "<!ENTITY % ninedef \"&#60;!ENTITY nine &#34;123456789&#34;&#62;\">\n"
       "%ninedef;\n"
       "]>\n"
       "<r>&nine;</r>",
       NULL, NULL,
       sizeof(XML_Char)
           * (strlen("<!ENTITY nine \"123456789\">") + strlen("123456789"))},
      {"<!DOCTYPE r [\n"
       "<!ENTITY % comment \"<!--1-->\">\n"
       "<!ENTITY % comment2 \"&#37;comment;<!--22-->&#37;comment;\">\n"
       "%comment2;\n"
       "]>\n"
       "<r/>\n",
       NULL, NULL,
       sizeof(XML_Char)
           * (strlen("%comment;<!--22-->%comment;") + 2 * strlen("<!--1-->"))},
      {"<!DOCTYPE r [\n"
       "  <!ENTITY % five \"12345\">\n"
       "  <!ENTITY % five2def \"&#60;!ENTITY five2 &#34;[&#37;five;][&#37;five;]]]]&#34;&#62;\">\n"
       "  %five2def;\n"
       "]>\n"
       "<r>&five2;</r>",
       NULL, NULL, /* from "%five2def;": */
       sizeof(XML_Char)
           * (strlen("<!ENTITY five2 \"[%five;][%five;]]]]\">")
              + 2 /* calls to "%five;" */ * strlen("12345")
              + /* from "&five2;": */ strlen("[12345][12345]]]]"))},
      {"<!DOCTYPE r SYSTEM \"first.ent\">\n"
       "<r/>",
       "<!ENTITY % comment '<!--1-->'>\n"
       "<!ENTITY % comment2 '<!--22-->%comment;<!--22-->%comment;<!--22-->'>\n"
       "%comment2;",
       NULL,
       sizeof(XML_Char)
           * (strlen("<!--22-->%comment;<!--22-->%comment;<!--22-->")
              + 2 /* calls to "%comment;" */ * strlen("<!---->"))},
      {"<!DOCTYPE r SYSTEM 'first.ent'>\n"
       "<r/>",
       "<!ENTITY % e1 PUBLIC 'foo' 'second.ent'>\n"
       "<!ENTITY % e2 '<!--22-->%e1;<!--22-->'>\n"
       "%e2;\n",
       "<!--1-->", sizeof(XML_Char) * strlen("<!--22--><!--1--><!--22-->")},
      {
          "<!DOCTYPE r SYSTEM 'first.ent'>\n"
          "<r/>",
          "<!ENTITY % e1 SYSTEM 'second.ent'>\n"
          "<!ENTITY % e2 '%e1;'>",
          "<?xml version='1.0' encoding='utf-8'?>\n"
          "hello\n"
          "xml" /* without trailing newline! */,
          0,
      },
      {
          "<!DOCTYPE r SYSTEM 'first.ent'>\n"
          "<r/>",
          "<!ENTITY % e1 SYSTEM 'second.ent'>\n"
          "<!ENTITY % e2 '%e1;'>",
          "<?xml version='1.0' encoding='utf-8'?>\n"
          "hello\n"
          "xml\n" /* with trailing newline! */,
          0,
      },
      {"<!DOCTYPE doc SYSTEM 'first.ent'>\n"
       "<doc></doc>\n",
       "<!ELEMENT doc EMPTY>\n"
       "<!ENTITY % e1 SYSTEM 'second.ent'>\n"
       "<!ENTITY % e2 '%e1;'>\n"
       "%e1;\n",
       "\xEF\xBB\xBF<!ATTLIST doc a1 CDATA 'value'>" /* UTF-8 BOM */,
       strlen("\xEF\xBB\xBF<!ATTLIST doc a1 CDATA 'value'>")},
#  endif /* XML_DTD */
  };

  const size_t countCases = sizeof(cases) / sizeof(cases[0]);
  size_t u = 0;
  for (; u < countCases; u++) {
    const unsigned long long expectedCountBytesDirect
        = strlen(cases[u].primaryText);
    const unsigned long long expectedCountBytesIndirect
        = (cases[u].firstExternalText ? strlen(cases[u].firstExternalText) : 0)
          + (cases[u].secondExternalText ? strlen(cases[u].secondExternalText)
                                         : 0)
          + cases[u].expectedCountBytesIndirectExtra;

    XML_Parser parser = XML_ParserCreate(NULL);
    XML_SetParamEntityParsing(parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
    if (cases[u].firstExternalText) {
      XML_SetExternalEntityRefHandler(parser,
                                      accounting_external_entity_ref_handler);
      XML_SetUserData(parser, (void *)&cases[u]);
    }

    enum XML_Status status
        = XML_Parse_SINGLE_BYTES(parser, cases[u].primaryText,
                                 (int)strlen(cases[u].primaryText), XML_TRUE);
    if (status != XML_STATUS_OK) {
      _xml_failure(parser, __FILE__, __LINE__);
    }

    const unsigned long long actualCountBytesDirect
        = testingAccountingGetCountBytesDirect(parser);
    const unsigned long long actualCountBytesIndirect
        = testingAccountingGetCountBytesIndirect(parser);

    XML_ParserFree(parser);

    if (actualCountBytesDirect != expectedCountBytesDirect) {
      fprintf(
          stderr,
          "Document " EXPAT_FMT_SIZE_T("") " of " EXPAT_FMT_SIZE_T("") ": Expected " EXPAT_FMT_ULL(
              "") " count direct bytes, got " EXPAT_FMT_ULL("") " instead.\n",
          u + 1, countCases, expectedCountBytesDirect, actualCountBytesDirect);
      fail("Count of direct bytes is off");
    }

    if (actualCountBytesIndirect != expectedCountBytesIndirect) {
      fprintf(
          stderr,
          "Document " EXPAT_FMT_SIZE_T("") " of " EXPAT_FMT_SIZE_T("") ": Expected " EXPAT_FMT_ULL(
              "") " count indirect bytes, got " EXPAT_FMT_ULL("") " instead.\n",
          u + 1, countCases, expectedCountBytesIndirect,
          actualCountBytesIndirect);
      fail("Count of indirect bytes is off");
    }
  }
}
END_TEST

START_TEST(test_billion_laughs_attack_protection_api) {
  XML_Parser parserWithoutParent = XML_ParserCreate(NULL);
  XML_Parser parserWithParent = XML_ExternalEntityParserCreate(
      parserWithoutParent, XCS("entity123"), NULL);
  if (parserWithoutParent == NULL)
    fail("parserWithoutParent is NULL");
  if (parserWithParent == NULL)
    fail("parserWithParent is NULL");

  // XML_SetBillionLaughsAttackProtectionMaximumAmplification, error cases
  if (XML_SetBillionLaughsAttackProtectionMaximumAmplification(NULL, 123.0f)
      == XML_TRUE)
    fail("Call with NULL parser is NOT supposed to succeed");
  if (XML_SetBillionLaughsAttackProtectionMaximumAmplification(parserWithParent,
                                                               123.0f)
      == XML_TRUE)
    fail("Call with non-root parser is NOT supposed to succeed");
  if (XML_SetBillionLaughsAttackProtectionMaximumAmplification(
          parserWithoutParent, NAN)
      == XML_TRUE)
    fail("Call with NaN limit is NOT supposed to succeed");
  if (XML_SetBillionLaughsAttackProtectionMaximumAmplification(
          parserWithoutParent, -1.0f)
      == XML_TRUE)
    fail("Call with negative limit is NOT supposed to succeed");
  if (XML_SetBillionLaughsAttackProtectionMaximumAmplification(
          parserWithoutParent, 0.9f)
      == XML_TRUE)
    fail("Call with positive limit <1.0 is NOT supposed to succeed");

  // XML_SetBillionLaughsAttackProtectionMaximumAmplification, success cases
  if (XML_SetBillionLaughsAttackProtectionMaximumAmplification(
          parserWithoutParent, 1.0f)
      == XML_FALSE)
    fail("Call with positive limit >=1.0 is supposed to succeed");
  if (XML_SetBillionLaughsAttackProtectionMaximumAmplification(
          parserWithoutParent, 123456.789f)
      == XML_FALSE)
    fail("Call with positive limit >=1.0 is supposed to succeed");
  if (XML_SetBillionLaughsAttackProtectionMaximumAmplification(
          parserWithoutParent, INFINITY)
      == XML_FALSE)
    fail("Call with positive limit >=1.0 is supposed to succeed");

  // XML_SetBillionLaughsAttackProtectionActivationThreshold, error cases
  if (XML_SetBillionLaughsAttackProtectionActivationThreshold(NULL, 123)
      == XML_TRUE)
    fail("Call with NULL parser is NOT supposed to succeed");
  if (XML_SetBillionLaughsAttackProtectionActivationThreshold(parserWithParent,
                                                              123)
      == XML_TRUE)
    fail("Call with non-root parser is NOT supposed to succeed");

  // XML_SetBillionLaughsAttackProtectionActivationThreshold, success cases
  if (XML_SetBillionLaughsAttackProtectionActivationThreshold(
          parserWithoutParent, 123)
      == XML_FALSE)
    fail("Call with non-NULL parentless parser is supposed to succeed");

  XML_ParserFree(parserWithParent);
  XML_ParserFree(parserWithoutParent);
}
END_TEST

START_TEST(test_billions_laughs_attack_protection_api_float_round_trip) {
  const float values[] = {1.0f, 1.1f, INFINITY};

  for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
    set_subtest("float %f", (double)i);
    const float wanted = values[i];

    XML_Parser parser = XML_ParserCreate(NULL);
    assert_true(parser != NULL);

    // Self-Test: Original value not already at target value
    double actual = 123.456;
    assert_true((float)actual != wanted);
#  if XML_GE == 1
    assert_true(
        XML_GetPropertyDouble(
            parser, XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION, &actual)
        == XML_PROP_ERROR_NONE);
    assert_true((float)actual != wanted); // Self-Test

    // Test: Target value applied successfully
    assert_true(
        XML_SetBillionLaughsAttackProtectionMaximumAmplification(parser, wanted)
        == XML_TRUE);
    assert_true(
        XML_GetPropertyDouble(
            parser, XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION, &actual)
        == XML_PROP_ERROR_NONE);
    assert_true((float)actual == wanted);
#  endif // XML_GE == 1
    XML_ParserFree(parser);
  }
}
END_TEST

START_TEST(test_helper_unsigned_char_to_printable) {
  // Smoke test
  unsigned char uc = 0;
  for (;; uc++) {
    set_subtest("char %u", (unsigned)uc);
    const char *const printable = unsignedCharToPrintable(uc);
    if (printable == NULL)
      fail("unsignedCharToPrintable returned NULL");
    else if (strlen(printable) < (size_t)1)
      fail("unsignedCharToPrintable returned empty string");
    if (uc == (unsigned char)-1) {
      break;
    }
  }

  // Two concrete samples
  set_subtest("char 'A'");
  if (strcmp(unsignedCharToPrintable('A'), "A") != 0)
    fail("unsignedCharToPrintable result mistaken");
  set_subtest("char '\\'");
  if (strcmp(unsignedCharToPrintable('\\'), "\\\\") != 0)
    fail("unsignedCharToPrintable result mistaken");
}
END_TEST

START_TEST(test_amplification_isolated_external_parser) {
  // NOTE: Length 44 is precisely twice the length of "<!ENTITY a SYSTEM 'b'>"
  // (22) that is used in function accountingGetCurrentAmplification in
  // xmlparse.c.
  //                  1.........1.........1.........1.........1..4 => 44
  const char doc[] = "<!ENTITY % p1 '123456789_123456789_1234567'>";
  const int docLen = (int)sizeof(doc) - 1;
  const float maximumToleratedAmplification = 2.0f;

  struct TestCase {
    int offsetOfThreshold;
    enum XML_Status expectedStatus;
  };

  struct TestCase cases[] = {
      {-2, XML_STATUS_ERROR}, {-1, XML_STATUS_ERROR}, {0, XML_STATUS_ERROR},
      {+1, XML_STATUS_OK},    {+2, XML_STATUS_OK},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    const int offsetOfThreshold = cases[i].offsetOfThreshold;
    const enum XML_Status expectedStatus = cases[i].expectedStatus;
    const unsigned long long activationThresholdBytes
        = docLen + offsetOfThreshold;

    set_subtest("offsetOfThreshold=%d, expectedStatus=%d", offsetOfThreshold,
                expectedStatus);

    XML_Parser parser = XML_ParserCreate(NULL);
    assert_true(parser != NULL);

    assert_true(XML_SetBillionLaughsAttackProtectionMaximumAmplification(
                    parser, maximumToleratedAmplification)
                == XML_TRUE);
    assert_true(XML_SetBillionLaughsAttackProtectionActivationThreshold(
                    parser, activationThresholdBytes)
                == XML_TRUE);

    XML_Parser ext_parser = XML_ExternalEntityParserCreate(parser, NULL, NULL);
    assert_true(ext_parser != NULL);

    const enum XML_Status actualStatus
        = XML_Parse_SINGLE_BYTES(ext_parser, doc, docLen, XML_TRUE);

    assert_true(actualStatus == expectedStatus);
    if (actualStatus != XML_STATUS_OK) {
      assert_true(XML_GetErrorCode(ext_parser)
                  == XML_ERROR_AMPLIFICATION_LIMIT_BREACH);
    }

    XML_ParserFree(ext_parser);
    XML_ParserFree(parser);
  }
}
END_TEST

START_TEST(test_amplification_reset_neutrality) {
  // Speed up the test, chunk size expected to not matter in this test
  if (g_chunkSize != 0)
    return;

  const char *const doc
      = "<!DOCTYPE r [<!ENTITY e 'general entity'>]><r>&e;</r>";
  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(XML_SetPropertyDouble(
                  parser, XML_PROP_BILLION_LAUGHS_MAXIMUM_AMPLIFICATION, 1.0)
              == XML_PROP_ERROR_NONE);
  assert_true(XML_SetPropertyUInt64(
                  parser, XML_PROP_BILLION_LAUGHS_ACTIVATION_THRESHOLD, 0)
              == XML_PROP_ERROR_NONE);

  assert_true(XML_Parse_SINGLE_BYTES(parser, doc, (int)strlen(doc),
                                     /*isFinal=*/XML_TRUE)
              == XML_STATUS_ERROR);
  assert_true(XML_GetErrorCode(parser) == XML_ERROR_AMPLIFICATION_LIMIT_BREACH);

  assert_true(XML_ParserReset(parser, NULL) == XML_STATUS_OK);

  assert_true(XML_Parse_SINGLE_BYTES(parser, doc, (int)strlen(doc),
                                     /*isFinal=*/XML_TRUE)
              == XML_STATUS_ERROR);
  assert_true(XML_GetErrorCode(parser) == XML_ERROR_AMPLIFICATION_LIMIT_BREACH);

  XML_ParserFree(parser);
}
END_TEST

#endif // XML_GE == 1

void
make_accounting_test_case(Suite *s) {
#if XML_GE == 1
  TCase *tc_accounting = tcase_create("accounting tests");

  suite_add_tcase(s, tc_accounting);

  tcase_add_test(tc_accounting, test_accounting_precision);
  tcase_add_test(tc_accounting, test_billion_laughs_attack_protection_api);
  tcase_add_test(tc_accounting,
                 test_billions_laughs_attack_protection_api_float_round_trip);
  tcase_add_test(tc_accounting, test_helper_unsigned_char_to_printable);
  tcase_add_test__ifdef_xml_dtd(tc_accounting,
                                test_amplification_isolated_external_parser);
  tcase_add_test__ifdef_xml_dtd(
      tc_accounting, test_external_entity_resource_budget_property);
  tcase_add_test__ifdef_xml_dtd(
      tc_accounting, test_external_entity_resource_budget_nested_entities);
  tcase_add_test__ifdef_xml_dtd(
      tc_accounting, test_external_entity_resource_budget_creation_failure);
  tcase_add_test(tc_accounting, test_amplification_reset_neutrality);
#else
  UNUSED_P(s);
#endif /* XML_GE == 1 */
}
