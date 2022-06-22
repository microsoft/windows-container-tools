//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "./pch.h"

/// JsonFileParser.cpp
///
/// Defines a class to parse JSON from a string buffer.
///


///
/// Parses a string at the current position of the buffer.
///
/// \return The string value. The value's content is temporary.
///
const std::wstring&
JsonFileParser::ParseStringValue()
{
    size_t offset = 0;
    int ch = PeekNextCharacter(offset);

    m_stringValue.clear();

    if (ch == '"')
    {
        offset++;

        while(true)
        {
            ch = PeekNextCharacter(offset);
            switch (ch)
            {
                case '\\':
                    ch = PeekNextCharacter(offset + 1);
                    if (ch == 'u')
                    {
                        m_stringValue += ParseControlCharacter(offset + 1);
                        offset += 6;
                    }
                    else
                    {
                        m_stringValue += ParseSpecialCharacter(ch);
                        offset += 2;
                    }

                    break;

                case '"':
                    //
                    // End of string.
                    //
                    offset++;
                    AdvanceBufferPointer(offset);
                    return m_stringValue;

                case EndOfBuffer:
                    //
                    // Could not find matching '"'.
                    //
                    throw std::invalid_argument("JsonFileParser: Reached EOF");

                default:
                    m_stringValue += static_cast<wchar_t>(ch);
                    offset++;
            }
        }
    }
    else
    {
        throw std::invalid_argument("JsonFileParser: Expected string value");
    }
}


///
/// Parses a escape sequence control character.
///
/// \return The control character value.
///
wchar_t
JsonFileParser::ParseControlCharacter(size_t Offset)
{
    int digit1 = HexToInt(PeekNextCharacter(Offset + 1));
    int digit2 = HexToInt(PeekNextCharacter(Offset + 2));
    int digit3 = HexToInt(PeekNextCharacter(Offset + 3));
    int digit4 = HexToInt(PeekNextCharacter(Offset + 4));

    //
    // Contol characters are in the form \XXXX, where X is and hex digit.
    // Fail if it is not an hex digit.
    //
    if (digit1 < 0 || digit2 < 0 || digit3 < 0 || digit4 < 0)
    {
        throw std::invalid_argument("JsonFileParser: Invalid escape sequence");
    }

    wchar_t ch = static_cast<wchar_t>((digit1 << 12) | (digit2 << 8) | (digit3 << 4) | digit4);

    return ch;
}


///
/// Parses a escape sequence special character.
///
/// \return The special character value.
///
wchar_t
JsonFileParser::ParseSpecialCharacter(int Ch)
{
    switch (Ch)
    {
    case '"':
    case '\\':
    case '/':
        break;

    case 'b':
        Ch = '\b';
        break;

    case 'f':
        Ch = '\f';
        break;

    case 'n':
        Ch = '\n';
        break;

    case 'r':
        Ch = '\r';
        break;

    case 't':
        Ch = '\t';
        break;

    default:
        throw std::invalid_argument("JsonFileParser: Invalid escape sequence");
    }

    return (static_cast<wchar_t>(Ch));
}


///
/// Skips a number at the current position of the buffer.
///
/// \return None.
///
void
JsonFileParser::SkipNumberValue()
{
    size_t offset = 0;

    int ch = PeekNextCharacter(offset);
    if (PeekNextCharacter(offset) == '-')
    {
        offset++;
    }

    ch = PeekNextCharacter(offset);

    if (ch >= '0' && ch <= '9')
    {
        do
        {
            offset++;
            ch = PeekNextCharacter(offset);
        } while (ch >= '0' && ch <= '9');
    }
    else
    {
        throw std::invalid_argument("JsonFileParser: Invalid numeric value");
    }

    if (ch == '.')
    {
        offset++;
        ch = PeekNextCharacter(offset);
        if (ch < '0' || ch > '9')
        {
            throw std::invalid_argument("JsonFileParser: Invalid numeric value");
        }

        do
        {
            offset++;
            ch = PeekNextCharacter(offset);
        } while (ch >= '0' && ch <= '9');
    }

    if (ch == 'e' || ch == 'E')
    {
        offset++;
        ch = PeekNextCharacter(offset);

        if (ch == '+' || ch == '-')
        {
            offset++;
            ch = PeekNextCharacter(offset);
        }

        if (ch < '0' || ch > '9')
        {
            throw std::invalid_argument("JsonFileParser: Invalid numeric value");
        }

        do
        {
            offset++;
            ch = PeekNextCharacter(offset);
        } while (ch >= '0' && ch <= '9');
    }

    AdvanceBufferPointer(offset);
}

///
/// Parses a null value at the current position of the buffer.
///
/// \return None.
///
void
JsonFileParser::ParseNullValue()
{
    if (PeekNextCharacter(0) == 'n' &&
        PeekNextCharacter(1) == 'u' &&
        PeekNextCharacter(2) == 'l' &&
        PeekNextCharacter(3) == 'l')
    {
        AdvanceBufferPointer(4);
    }
    else
    {
        throw std::invalid_argument("JsonFileParser: Expected null value");
    }
}

///
/// Parses a boolean value at the current position of the buffer.
///
/// \return A boolean value, corresponding to its textual representation.
///
bool
JsonFileParser::ParseBooleanValue()
{
    if (PeekNextCharacter(0) == 't' &&
        PeekNextCharacter(1) == 'r' &&
        PeekNextCharacter(2) == 'u' &&
        PeekNextCharacter(3) == 'e')
    {
        AdvanceBufferPointer(4);
        return true;
    }
    else if (PeekNextCharacter(0) == 'f' &&
             PeekNextCharacter(1) == 'a' &&
             PeekNextCharacter(2) == 'l' &&
             PeekNextCharacter(3) == 's' &&
             PeekNextCharacter(4) == 'e')
    {
        AdvanceBufferPointer(5);
        return false;
    }
    else
    {
        throw std::invalid_argument("JsonFileParser: Expected boolean value");
    }
}


///
/// Starts parsing the beginning of an array.
///
/// \return A boolean that's true if the array has content. False, otherwise.
///
bool
JsonFileParser::BeginParseArray()
{
    if (PeekNextCharacter() != '[')
    {
        throw std::invalid_argument("JsonFileParser: Error at beginning of an object.");
    }

    AdvanceBufferPointer(1);

    if (PeekNextCharacter() == ']')
    {
        AdvanceBufferPointer(1);
        return false;
    }

    return true;
}

///
/// Continues reading the buffer until the next element of the array is found.
///
/// \return A boolean that's true if the there is another element. False, otherwise.
///
bool
JsonFileParser::ParseNextArrayElement()
{
    int ch = PeekNextCharacter();
    if (ch == ']')
    {
        AdvanceBufferPointer(1);
        return false;
    }
    else if (ch == ',')
    {
        AdvanceBufferPointer(1);
        return true;
    }
    else
    {
        throw std::invalid_argument("JsonFileParser: Error at end of an array.");
    }
}

///
/// Starts parsing the beginning of an object.
///
/// \return A boolean that's true if the object has content. False, otherwise.
///
bool
JsonFileParser::BeginParseObject()
{
    if (PeekNextCharacter() != '{')
    {
        throw std::invalid_argument("JsonFileParser: Error at beginning of an array.");
    }

    AdvanceBufferPointer(1);

    if (PeekNextCharacter() == '}')
    {
        AdvanceBufferPointer(1);
        return false;
    }

    ParseKey();

    return true;
}

///
/// Continues reading the buffer until the next object element is found.
///
/// \return A boolean that's true if the there is another element. False, otherwise.
///
bool
JsonFileParser::ParseNextObjectElement()
{
    int ch = PeekNextCharacter();
    if (ch == '}')
    {
        AdvanceBufferPointer(1);
        return false;
    }
    else if (ch == ',')
    {
        AdvanceBufferPointer(1);
        ParseKey();
        return true;
    }
    else
    {
        throw std::invalid_argument("JsonFileParser: Error at end of an object.");
    }
}


///
/// Parses the key of an object, storing the key, and skipping the ':' separator.
///
/// \return None.
///
void
JsonFileParser::ParseKey()
{
    m_key = ParseStringValue();
    if (PeekNextCharacter() != ':')
    {
        throw std::invalid_argument("JsonFileParser: Expected an object separator ':'.");
    }

    AdvanceBufferPointer(1);
}

///
/// Returns the key of the last read object.
///
/// \return A string with the key. It returns a temporary object.
///
const std::wstring&
JsonFileParser::GetKey()
{
    return m_key;
}

///
/// Identifies the type of the next value in the buffer the key of the last read object.
///
/// \return A DataType enum value with the identified type.
///
JsonFileParser::DataType
JsonFileParser::GetNextDataType()
{
    DataType type;
    int ch = PeekNextCharacter();

    switch (ch)
    {
        case '{':
            type = DataType::Object;
            break;

        case '[':
            type = DataType::Array;
            break;

        case '"':
            type = DataType::String;
            break;

        case 't':
        case 'f':
            type = DataType::Boolean;
            break;

        case 'n':
            type = DataType::Null;
            break;

        default:
            if ((ch >= '0' && ch <= '9') || ch == '-')
            {
                type = DataType::Number;
            }
            else
            {
                throw std::invalid_argument("JsonFileParser: Error reading a valid value.");
            }

            break;
    }

    return type;
}

///
/// Skips the value of the next object.
///
/// \return None.
///
void
JsonFileParser::SkipValue()
{
    std::vector<int> valueStack;

    do
    {
        bool first = false;
        switch (GetNextDataType())
        {
            case DataType::Array:
                if (BeginParseArray())
                {
                    valueStack.push_back('[');
                    first = true;
                }

                break;

            case DataType::Object:
                if (BeginParseObject())
                {
                    valueStack.push_back('{');
                    first = true;
                }

                break;

            case DataType::Boolean:
                ParseBooleanValue();
                break;

            case DataType::Number:
                SkipNumberValue();
                break;

            case DataType::String:
                ParseStringValue();
                break;

            case DataType::Null:
                ParseNullValue();
                break;
        }

        if (!first)
        {
            while (!valueStack.empty())
            {
                if (valueStack.back() == '{')
                {
                    if (ParseNextObjectElement())
                    {
                        break;
                    }
                }
                else if (valueStack.back() == '[')
                {
                    if (ParseNextArrayElement())
                    {
                        break;
                    }
                }
                else
                {
                    throw std::invalid_argument("JsonFileParser: Invalid value.");
                }

                valueStack.pop_back();
            }
        }
    } while (!valueStack.empty());
}

///
/// Gets a character from the buffer at the position m_currentPos + Offset.
///
/// \return An integer with the value of the character, or -1
///     if the end of the buffer was reached.
///
int
JsonFileParser::PeekNextCharacter(
    size_t Offset
    )
{
    if (m_currentPos + Offset < m_bufferLength)
    {
        return m_buffer[m_currentPos + Offset];
    }
    else
    {
        return EndOfBuffer;
    }
}


///
/// Go to the next character at specified offset. Skip whitespaces and new line
/// characters at that position.
///
/// \return None.
///
void
JsonFileParser::AdvanceBufferPointer(
    size_t Offset
    )
{
    m_currentPos = min(m_currentPos + Offset, m_bufferLength);

    for (; m_currentPos < m_bufferLength && IsWhiteSpace(m_buffer[m_currentPos]); m_currentPos++)
    {
        //
        // Skip whitespaces
        //
    }
}


///
/// Converts a hexadecimal digit to a decimal number.
///
/// \param int        The hexadecimal character
///
/// \return An integer between 0 to 9, or -1, if the hexadecimal digit is invalid
///
inline
int
JsonFileParser::HexToInt(
    _In_ int HexChar
    )
{
    int retVal = -1;

    if (HexChar >= '0' && HexChar <= '9')
    {
        retVal = HexChar - '0';
    }
    else if (HexChar >= 'A' && HexChar <= 'F')
    {
        retVal = HexChar - 'A' + 10;
    }
    else if (HexChar >= 'a' && HexChar <= 'f')
    {
        retVal = HexChar - 'a' + 10;
    }

    return retVal;
}

///
/// Checks if specified character is whitespace character
///
/// \return true if whitespace character, false otherwise
///
inline
bool
JsonFileParser::IsWhiteSpace(
    _In_ int Character
    )
{
    switch (Character)
    {
    //
    // Skip new line and whitespace characters
    //
    case ' ':
    case '\r':
    case '\n':
    case '\t':
        return true;

    default:
        return false;
    }
}
