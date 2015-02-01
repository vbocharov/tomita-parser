#pragma once

#include <util/charset/doccodes.h>
#include <util/generic/noncopyable.h>
#include <util/generic/ptr.h>
#include <util/generic/stroka.h>
#include <util/generic/vector.h>
#include <util/generic/yexception.h>
#include <util/stream/str.h>

namespace NJson {
    class TJsonValue;
}

namespace NJsonWriter {

    enum EJsonEntity {
        JE_OUTER_SPACE = 1,
        JE_LIST,
        JE_OBJECT,
        JE_PAIR,
    };

    enum EHtmlEscapeMode {
        HEM_ESCAPE_HTML = 1,  // Use HTML escaping: &lt; &gt; &amp; \/
        HEM_DONT_ESCAPE_HTML, // Use JSON escaping: \u003C \u003E \u0026 \/
        HEM_RELAXED,          // Use JSON escaping: \u003C \u003E \u0026 /
        HEM_UNSAFE,           // Turn escaping off: < > & /
    };

    class TError: public yexception {};

    class TValueContext;
    class TPairContext;
    class TAfterColonContext;

    class TBuf: TNonCopyable {
    public:
        TBuf(EHtmlEscapeMode mode = HEM_DONT_ESCAPE_HTML, TOutputStream* stream = 0);

        TValueContext WriteString(const TStringBuf& s, EHtmlEscapeMode hem);
        TValueContext WriteString(const TStringBuf& s);
        TValueContext WriteInt(int i);
        TValueContext WriteLongLong(long long i);
        TValueContext WriteULongLong(unsigned long long i);
        TValueContext WriteFloat(float f);
        TValueContext WriteDouble(double f);
        TValueContext WriteBool(bool b);
        TValueContext WriteNull();
        TValueContext WriteJsonValue(const NJson::TJsonValue* value, bool sortKeys = false);

        TValueContext BeginList();
        TBuf& EndList();

        TPairContext BeginObject();
        TAfterColonContext WriteKey(const TStringBuf& key, EHtmlEscapeMode hem);
        TAfterColonContext WriteKey(const TStringBuf& key);
        bool KeyExpected() const {
            return Stack.back() == JE_OBJECT;
        }

        //! deprecated, do not use in new code
        TAfterColonContext CompatWriteKeyWithoutQuotes(const TStringBuf& key);

        TBuf& EndObject();

        /*** Indent the resulting JSON with spaces.
           * By default (spaces==0) no formatting is done.                */
        void SetIndentSpaces(int spaces) {
            IndentSpaces = spaces;
        }

        void SetEncoding(ECharset charset) {
            Charset = charset;
        }

        /*** Return the string formed in the internal TStringStream.
           * You may only call it if the `stream' parameter was NULL
           * at construction time.                                        */
        const Stroka& Str() const;

        /*** Dump and forget the string constructed so far.
           * You may only call it if the `stream' parameter was NULL
           * at construction time.                                        */
        void FlushTo(TOutputStream* stream);

        /*** Write a literal string that represents a JSON value
           * (string, number, object, array, bool, or null).
           *
           * Example:
           * j.UnsafeWriteValue("[1, 2, 3, \"o'clock\", 4, \"o'clock rock\"]");
           *
           * As in all of the Unsafe* functions, no escaping is done.     */
        void UnsafeWriteValue(const TStringBuf& s);
        void UnsafeWriteValue(const char* s, size_t len);

        /*** When in the context of an object, write a literal string
           * that represents a key:value pair (or several pairs).
           *
           * Example:
           * j.BeginObject();
           * j.UnsafeWritePair("\"adam\": \"male\", \"eve\": \"female\"");
           * j.EndObject();
           *
           * As in all of the Unsafe* functions, no escaping is done.     */
        TPairContext UnsafeWritePair(const TStringBuf& s);

        /*** Copy the supplied string directly into the output stream.    */
        void UnsafeWriteRawBytes(const TStringBuf& s);
        void UnsafeWriteRawBytes(const char* c, size_t len);

    private:
        void BeginValue();
        void EndValue();
        void BeginKey();
        void RawWriteChar(char c);
        bool EscapedWriteChar(const char* b, const char* c, EHtmlEscapeMode hem);
        void EscapedWriteStringContent(const TStringBuf& s, EHtmlEscapeMode hem);
        void WriteBareString(const TStringBuf& s, EHtmlEscapeMode hem);
        void WriteComma();
        void PrintIndentation(bool closing);
        void WriteHexEscape(unsigned char c);

        void StackPush(EJsonEntity e);
        void StackPop();
        void CheckAndPop(EJsonEntity e);
        EJsonEntity StackTop() const;


        TOutputStream* Stream;
        THolder<TStringStream> StringStream;
        yvector<EJsonEntity> Stack;
        bool NeedComma;
        bool NeedNewline;
        const EHtmlEscapeMode EscapeMode;
        int IndentSpaces;
        ECharset Charset;
    };

    // Please don't try to instantiate the classes declared below this point.

    template <typename TOutContext>
    class TValueWriter {
    public:
        TOutContext WriteString(const TStringBuf& s, EHtmlEscapeMode hem) {
            Buf.WriteString(s, hem);
            return TOutContext(Buf); }
#define JSON_VALUE_WRITER_WRAP(function, type) \
        TOutContext function(type arg) { \
            Buf.function(arg); \
            return TOutContext(Buf); \
        }
        JSON_VALUE_WRITER_WRAP(WriteString, const TStringBuf&)
        JSON_VALUE_WRITER_WRAP(WriteInt, int)
        JSON_VALUE_WRITER_WRAP(WriteLongLong, long long)
        JSON_VALUE_WRITER_WRAP(WriteULongLong, unsigned long long)
        JSON_VALUE_WRITER_WRAP(WriteFloat, float)
        JSON_VALUE_WRITER_WRAP(WriteDouble, double)
        JSON_VALUE_WRITER_WRAP(WriteBool, bool)
        JSON_VALUE_WRITER_WRAP(UnsafeWriteValue, const TStringBuf&)
#undef  JSON_VALUE_WRITER_WRAP
        TValueContext BeginList();
        TPairContext BeginObject();

    protected:
        TValueWriter(TBuf& buf)
            : Buf(buf)
        {}
        friend class TBuf;

    protected:
        TBuf& Buf;
    };

    class TValueContext: public TValueWriter<TValueContext> {
    public:
        TBuf& EndList() {
            return Buf.EndList();
        }
        Stroka Str() const { return Buf.Str(); }
    private:
        TValueContext(TBuf& buf)
            : TValueWriter<TValueContext>(buf)
        {}
        friend class TBuf;
        friend class TValueWriter<TValueContext>;
    };

    class TAfterColonContext: public TValueWriter<TPairContext> {
    private:
        TAfterColonContext(TBuf& iBuf): TValueWriter<TPairContext>(iBuf) {}
        friend class TBuf;
        friend class TPairContext;
    };

    class TPairContext {
    public:
        TAfterColonContext WriteKey(const TStringBuf& s, EHtmlEscapeMode hem) {
            return Buf.WriteKey(s, hem);
        }
        TAfterColonContext WriteKey(const TStringBuf& s) {
            return Buf.WriteKey(s);
        }
        TAfterColonContext CompatWriteKeyWithoutQuotes(const TStringBuf& s) {
            return Buf.CompatWriteKeyWithoutQuotes(s);
        }
        TPairContext UnsafeWritePair(const TStringBuf& s) {
            return Buf.UnsafeWritePair(s);
        }
        TBuf& EndObject() {
            return Buf.EndObject();
        }

    private:
        TPairContext(TBuf& buf)
            : Buf(buf)
        {
        }

        friend class TBuf;
        friend class TValueWriter<TPairContext>;

    private:
        TBuf& Buf;
    };

    template<typename TOutContext>
        TValueContext TValueWriter<TOutContext>::BeginList() {
        return Buf.BeginList();
    }

    template<typename TOutContext>
        TPairContext TValueWriter<TOutContext>::BeginObject() {
        return Buf.BeginObject();
    }
}