#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <type_traits>
#define IRAM_ATTR
#define HIGH 1
#define OUTPUT 1
#define FILE_READ "r"
#define FILE_WRITE "w"
// Periféricos simulados: não validam o circuito físico.
inline uint64_t mockTime=0;
inline uint32_t mockReadDelay=0;
inline bool mockWriteFails=false;
inline size_t mockUsedBytes=1000;
inline uint32_t millis(){return mockTime;}
inline void delay(int n){mockTime+=n;}
inline void yield(){}
inline void pinMode(int,int){}
inline void digitalWrite(int,int){}
inline int64_t esp_timer_get_time(){return int64_t(mockTime)*1000;}
template<class A,class B> auto min(A a,B b){using C=std::common_type_t<A,B>;return std::min(C(a),C(b));}
struct SerialMock {
 std::string output;
 void begin(int){}
 int available(){return 0;} int read(){return 0;}
 void println(const char*s){output+=s;output+='\n';}
 void printf(const char *fmt,...){char b[1024];va_list a;va_start(a,fmt);vsnprintf(b,sizeof(b),fmt,a);va_end(a);output+=b;}
};
inline SerialMock Serial;
struct SpiMock{void begin(int,int,int,int){}};inline SpiMock SPI;
struct HX711{
 bool ready=false;long raw=0;
 void begin(int,int,int){}
 bool is_ready(){return ready;}
 long read(){ready=false;mockTime+=mockReadDelay;return raw;}
};
struct File {
 std::shared_ptr<std::string> data;std::string filename;size_t position=0;
 explicit operator bool()const{return bool(data);}
 size_t size(){return data?data->size():0;}
 bool available(){return data&&position<data->size();}
 bool seek(size_t p){if(!data||p>data->size())return false;position=p;return true;}
 size_t read(uint8_t*out,size_t n){if(!data)return 0;n=std::min(n,data->size()-position);memcpy(out,data->data()+position,n);position+=n;return n;}
 size_t write(const uint8_t*in,size_t n){if(!data||mockWriteFails)return 0;if(position+n>data->size())data->resize(position+n);memcpy(data->data()+position,in,n);position+=n;return n;}
 size_t write(uint8_t byte){return write(&byte,1);}
 size_t readBytesUntil(char stop,char*out,size_t n){size_t i=0;while(available()&&i<n){char c=(*data)[position++];if(c==stop)break;out[i++]=c;}return i;}
 void flush(){}void close(){data.reset();}
 const char*name(){return filename.c_str();}File openNextFile(){return {};}
};
struct FsMock {
 std::map<std::string,std::shared_ptr<std::string>> files;
 bool begin(bool){return true;}size_t totalBytes(){return 2000000;}size_t usedBytes(){return mockUsedBytes;}
 bool exists(const char*p){return files.count(p);}
 File open(const char*p,const char*m="r"){
  if(*m=='w')files[p]=std::make_shared<std::string>();
  if(!files.count(p))return {};
  return File{files[p],p,0};
 }
 bool remove(const char*p){return files.erase(p);}
};inline FsMock LittleFS;
struct LoRaMock {
 std::string pending;std::vector<std::string> frames;
 void setPins(int,int,int){}void setSPIFrequency(long){}bool begin(long){return true;}
 void setSyncWord(int){}void setSpreadingFactor(int){}void setSignalBandwidth(long){}
 void setCodingRate4(int){}void setPreambleLength(int){}void disableCrc(){}
 void disableInvertIQ(){}void setTxPower(int){}void onTxDone(void(*)()){}void sleep(){}
 bool beginPacket(){pending.clear();return true;}
 size_t write(const uint8_t*data,size_t n){pending.append((const char*)data,n);return n;}
 bool endPacket(bool){frames.push_back(pending);return true;}
};inline LoRaMock LoRa;
inline int mbedtls_base64_encode(unsigned char*out,size_t cap,size_t*written,const unsigned char*in,size_t n){
 const char*abc="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 size_t len=((n+2)/3)*4;if(cap<=len)return -1;size_t j=0;
 for(size_t i=0;i<n;i+=3){uint32_t x=uint32_t(in[i])<<16;if(i+1<n)x|=uint32_t(in[i+1])<<8;if(i+2<n)x|=in[i+2];
  out[j++]=abc[(x>>18)&63];out[j++]=abc[(x>>12)&63];out[j++]=i+1<n?abc[(x>>6)&63]:'=';out[j++]=i+2<n?abc[x&63]:'=';}
 out[j]=0;*written=j;return 0;
}
