//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#define BUFFER_SIZE 65536

namespace UtilityTests
{
    ///
    /// Tests of Utility class methods.
    ///
    TEST_CLASS(UtilityTests)
    {
    public:
        TEST_METHOD(TestisJsonNumberTrue)
        {
            PWSTR str = L"-0.12";
            Assert::IsTrue(Utility::isJsonNumber(str), L"should return true -0.12");

            str = L"-1.1234";
            Assert::IsTrue(Utility::isJsonNumber(str), L"should return true for -1.1234");

            str = L"1.12";
            Assert::IsTrue(Utility::isJsonNumber(str), L"should return true for 1.12");

            str = L"1";
            Assert::IsTrue(Utility::isJsonNumber(str), L"should return true for 1");

            str = L"0";
            Assert::IsTrue(Utility::isJsonNumber(str), L"should return true for 0");

            str = L"456662";
            Assert::IsTrue(Utility::isJsonNumber(str), L"should return true for 456662");

            str = L"456662.8989";
            Assert::IsTrue(Utility::isJsonNumber(str), L"should return true for 456662.8989");
        }

        TEST_METHOD(TestisJsonNumberFalse)
        {
            PWSTR str = L"false";
            Assert::IsFalse(Utility::isJsonNumber(str), L"should return false for \"false\"");

            str = L"12.12.89.12";
            Assert::IsFalse(Utility::isJsonNumber(str), L"should return calse for 12.12.89.12");

            str = L"1200.23x";
            Assert::IsFalse(Utility::isJsonNumber(str), L"should return false for 1200.23x");
        }

        TEST_METHOD(TestSanitizeJson)
        {
            std::wstring str = L"say, \"hello\"";
            std::wstring expect = L"say, \\\"hello\\\"";
            Utility::SanitizeJson(str);
            Assert::IsTrue(str == expect, L"should escape \"");
            str = L"\"hello\"";
            expect = L"\\\"hello\\\"";
            Utility::SanitizeJson(str);
            Assert::IsTrue(str == expect, L"should escape \"");

            str = L"hello\r\nworld";
            expect = L"hello\\r\\nworld";
            Utility::SanitizeJson(str);
            Assert::IsTrue(str == expect, L"should escape \r and \n");
            str = L"\r\nHello\r\n";
            expect = L"\\r\\nHello\\r\\n";
            Utility::SanitizeJson(str);
            Assert::IsTrue(str == expect, L"should escape \r and \n");

            str = L"\\Driver\\XX\\";
            expect = L"\\\\Driver\\\\XX\\\\";
            Utility::SanitizeJson(str);
            Assert::IsTrue(str == expect, L"should escape \\");
            str = L"C:\\Drive\\XX";
            expect = L"C:\\\\Drive\\\\XX";
            Utility::SanitizeJson(str);
            Assert::IsTrue(str == expect, L"should escape \\");
        }
    };
}
