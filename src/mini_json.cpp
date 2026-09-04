#include "mini_json.hpp"
#include <charconv>
#include <cmath>

namespace kg::json {

bool Value::AsBool(bool fallback) const { return IsBool() ? std::get<bool>(data_) : fallback; }
double Value::AsNumber(double fallback) const { return IsNumber() ? std::get<double>(data_) : fallback; }
const std::string& Value::AsString() const { static const std::string empty; return IsString() ? std::get<std::string>(data_) : empty; }
const Value::Array& Value::AsArray() const { static const Array empty; return IsArray() ? std::get<Array>(data_) : empty; }
Value::Array& Value::AsArray() { if (!IsArray()) data_ = Array{}; return std::get<Array>(data_); }
const Value::Object& Value::AsObject() const { static const Object empty; return IsObject() ? std::get<Object>(data_) : empty; }
Value::Object& Value::AsObject() { if (!IsObject()) data_ = Object{}; return std::get<Object>(data_); }
const Value* Value::Find(std::string_view key) const {
    if (!IsObject()) return nullptr;
    const auto& obj = std::get<Object>(data_);
    const auto it = obj.find(key);
    return it == obj.end() ? nullptr : &it->second;
}
Value* Value::Find(std::string_view key) {
    if (!IsObject()) return nullptr;
    auto& obj = std::get<Object>(data_);
    const auto it = obj.find(key);
    return it == obj.end() ? nullptr : &it->second;
}
Value& Value::operator[](std::string key) { return AsObject()[std::move(key)]; }

class Parser {
public:
    explicit Parser(std::string_view text) : s_(text) {}
    Value Run() {
        Skip();
        auto v = ParseValue();
        Skip();
        if (i_ != s_.size()) Error("Unerwartete Zeichen nach JSON-Wert");
        return v;
    }
private:
    std::string_view s_; size_t i_{};
    [[noreturn]] void Error(const char* msg) const { throw std::runtime_error(std::string(msg) + " bei Byte " + std::to_string(i_)); }
    void Skip() { while (i_ < s_.size() && (s_[i_]==' '||s_[i_]=='\n'||s_[i_]=='\r'||s_[i_]=='\t')) ++i_; }
    bool Eat(char c) { Skip(); if (i_ < s_.size() && s_[i_] == c) { ++i_; return true; } return false; }
    Value ParseValue() {
        Skip(); if (i_ >= s_.size()) Error("JSON unerwartet zu Ende");
        switch (s_[i_]) {
            case '{': return ParseObject(); case '[': return ParseArray(); case '"': return Value(ParseString());
            case 't': Expect("true"); return Value(true); case 'f': Expect("false"); return Value(false); case 'n': Expect("null"); return Value(nullptr);
            default: if (s_[i_]=='-' || (s_[i_] >= '0' && s_[i_] <= '9')) return Value(ParseNumber());
        }
        Error("Ungültiger JSON-Wert");
    }
    void Expect(std::string_view word) { if (s_.substr(i_, word.size()) != word) Error("Ungültiges Literal"); i_ += word.size(); }
    Value ParseObject() {
        Eat('{'); Value::Object obj; Skip(); if (Eat('}')) return Value(std::move(obj));
        for (;;) {
            Skip(); if (i_ >= s_.size() || s_[i_] != '"') Error("Objektschlüssel erwartet");
            auto key = ParseString(); if (!Eat(':')) Error(":" " erwartet");
            obj.emplace(std::move(key), ParseValue());
            if (Eat('}')) break; if (!Eat(',')) Error(", oder } erwartet");
        }
        return Value(std::move(obj));
    }
    Value ParseArray() {
        Eat('['); Value::Array arr; Skip(); if (Eat(']')) return Value(std::move(arr));
        for (;;) { arr.push_back(ParseValue()); if (Eat(']')) break; if (!Eat(',')) Error(", oder ] erwartet"); }
        return Value(std::move(arr));
    }
    static void AppendUtf8(std::string& out, unsigned cp) {
        if (cp <= 0x7F) out.push_back(static_cast<char>(cp));
        else if (cp <= 0x7FF) { out.push_back(char(0xC0 | (cp>>6))); out.push_back(char(0x80 | (cp&0x3F))); }
        else if (cp <= 0xFFFF) { out.push_back(char(0xE0 | (cp>>12))); out.push_back(char(0x80 | ((cp>>6)&0x3F))); out.push_back(char(0x80 | (cp&0x3F))); }
        else { out.push_back(char(0xF0 | (cp>>18))); out.push_back(char(0x80 | ((cp>>12)&0x3F))); out.push_back(char(0x80 | ((cp>>6)&0x3F))); out.push_back(char(0x80 | (cp&0x3F))); }
    }
    unsigned Hex4() {
        unsigned v=0; for (int n=0;n<4;++n) { if (i_>=s_.size()) Error("Unicode-Escape zu kurz"); const char c=s_[i_++]; v<<=4;
            if (c>='0'&&c<='9') v|=c-'0'; else if(c>='a'&&c<='f') v|=10+c-'a'; else if(c>='A'&&c<='F') v|=10+c-'A'; else Error("Ungültige Hex-Ziffer"); }
        return v;
    }
    std::string ParseString() {
        if (s_[i_++] != '"') Error("String erwartet"); std::string out;
        while (i_ < s_.size()) { const char c=s_[i_++]; if (c=='"') return out; if (c!='\\') { out.push_back(c); continue; }
            if (i_>=s_.size()) Error("Escape zu Ende"); const char e=s_[i_++];
            switch(e){case '"':out.push_back('"');break;case '\\':out.push_back('\\');break;case '/':out.push_back('/');break;case 'b':out.push_back('\b');break;case 'f':out.push_back('\f');break;case 'n':out.push_back('\n');break;case 'r':out.push_back('\r');break;case 't':out.push_back('\t');break;
                case 'u': { unsigned cp=Hex4(); if (cp>=0xD800&&cp<=0xDBFF && i_+2<s_.size() && s_[i_]=='\\' && s_[i_+1]=='u') { i_+=2; unsigned lo=Hex4(); if(lo>=0xDC00&&lo<=0xDFFF) cp=0x10000+((cp-0xD800)<<10)+(lo-0xDC00); } AppendUtf8(out,cp); break; }
                default: Error("Ungültiges Escape"); }
        }
        Error("String nicht geschlossen");
    }
    double ParseNumber() {
        const size_t start=i_; if(s_[i_]=='-')++i_; while(i_<s_.size()&&isdigit(static_cast<unsigned char>(s_[i_])))++i_;
        if(i_<s_.size()&&s_[i_]=='.'){++i_;while(i_<s_.size()&&isdigit(static_cast<unsigned char>(s_[i_])))++i_;}
        if(i_<s_.size()&&(s_[i_]=='e'||s_[i_]=='E')){++i_;if(i_<s_.size()&&(s_[i_]=='+'||s_[i_]=='-'))++i_;while(i_<s_.size()&&isdigit(static_cast<unsigned char>(s_[i_])))++i_;}
        double v{}; auto token=s_.substr(start,i_-start); auto [p,ec]=std::from_chars(token.data(),token.data()+token.size(),v); if(ec!=std::errc{}) Error("Ungültige Zahl"); return v;
    }
};

Value Parse(std::string_view text) { return Parser(text).Run(); }

std::string Escape(std::string_view text) {
    std::string out; out.reserve(text.size()+8); out.push_back('"');
    for (unsigned char c: text) { switch(c){case '"':out+="\\\"";break;case '\\':out+="\\\\";break;case '\b':out+="\\b";break;case '\f':out+="\\f";break;case '\n':out+="\\n";break;case '\r':out+="\\r";break;case '\t':out+="\\t";break; default: if(c<0x20){char b[7];sprintf_s(b,"\\u%04x",c);out+=b;}else out.push_back(static_cast<char>(c));}}
    out.push_back('"'); return out;
}
static void DumpImpl(const Value& v,std::string& out,int indent,int depth){
    auto pad=[&](int d){out.append(static_cast<size_t>(d*indent),' ');};
    if(v.IsNull()){out+="null";} else if(v.IsBool()){out+=v.AsBool()?"true":"false";} else if(v.IsNumber()){std::ostringstream ss;ss<<std::setprecision(15)<<v.AsNumber();out+=ss.str();}
    else if(v.IsString()){out+=Escape(v.AsString());}
    else if(v.IsArray()){out+='[';const auto&a=v.AsArray();if(!a.empty()){if(indent)out+='\n';for(size_t i=0;i<a.size();++i){if(indent)pad(depth+1);DumpImpl(a[i],out,indent,depth+1);if(i+1<a.size())out+=',';if(indent)out+='\n';}if(indent)pad(depth);}out+=']';}
    else {out+='{';const auto&o=v.AsObject();if(!o.empty()){if(indent)out+='\n';size_t n=0;for(const auto&[k,val]:o){if(indent)pad(depth+1);out+=Escape(k);out+=indent?": ":":";DumpImpl(val,out,indent,depth+1);if(++n<o.size())out+=',';if(indent)out+='\n';}if(indent)pad(depth);}out+='}';}
}
std::string Dump(const Value& value,int indent){std::string out;DumpImpl(value,out,std::max(0,indent),0);return out;}

} // namespace kg::json
