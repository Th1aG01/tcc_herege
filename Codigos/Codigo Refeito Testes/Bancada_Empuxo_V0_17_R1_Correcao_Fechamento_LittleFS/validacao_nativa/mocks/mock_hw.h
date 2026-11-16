#pragma once
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

constexpr int OUTPUT=1, LOW=0, HIGH=1;
inline unsigned long fakeMillis=0;
inline std::map<int,int> pins, modes;
inline unsigned long millis(){return fakeMillis;}
inline void delay(unsigned long n){fakeMillis+=n;}
inline void pinMode(int p,int m){modes[p]=m;}
inline void digitalWrite(int p,int v){pins[p]=v;}
struct SerialMock {
  std::deque<unsigned char> input;
  std::string output;
  int baud=0;
  int capacidadeTX=64;
  int availableForWrite(){return capacidadeTX;}
  void begin(int v){baud=v;}
  int available(){return (int)input.size();}
  int read(){auto v=input.front(); input.pop_front(); return v;}
  size_t write(uint8_t b){output+=(char)b; return 1;}
  size_t write(const uint8_t* b,size_t n){output.append((const char*)b,n);return n;}
  void println(){output+='\n';}
  void println(const char* s){output+=s;output+='\n';}
  void feed(const std::string& s){for(auto c:s)input.push_back((unsigned char)c);}
};
inline SerialMock Serial;
struct EspMock {uint64_t getEfuseMac(){return 0xAABBCCDDEEFFULL;}};
inline EspMock ESP;
#ifndef TEST_REAL_HX711_API
struct HX711 {
  bool ready=true;
  int remaining=-1, dt=-1, sck=-1;
  long value=1000, offset=0;
  void begin(int d,int s){dt=d;sck=s;}
  bool is_ready(){return ready && remaining!=0;}
  long read(){if(remaining>0)--remaining;return value;}
  double get_value(){return read()-offset;}
  void set_offset(long v){offset=v;}
};
#endif
struct SPIMock {
  std::vector<int> args;
  void begin(int a,int b,int c,int d){args={a,b,c,d};}
};
inline SPIMock SPI;
#ifdef TEST_REAL_LORA_API
using byte=uint8_t;
using SPIClass=SPIMock;
class SPISettings {};
class Stream {public: size_t print(const char*); virtual ~Stream()=default;};
#else
struct LoRaMock {
  bool ready=true, writeFail=false, endFail=false;
  unsigned long txEnd=0, txDuration=100, spiHz=0;
  double frequency=0;
  int sync=-1,power=-1;
  std::vector<int> pins;
  std::vector<std::string> packets;
  std::string current;
  void setPins(int a,int b,int c){pins={a,b,c};}
  void setSPIFrequency(unsigned long f){spiHz=f;}
  bool begin(double f){frequency=f;return ready;}
  void setSyncWord(int v){sync=v;}
  void setTxPower(int v){power=v;}
  void (*txDone)()=nullptr;
  bool pending=false;
  bool rxMode=false;
  std::deque<std::string> incoming;
  std::string rxCurrent;size_t rxPos=0;
  void feedPacket(const std::string& data){incoming.push_back(data);}
  int parsePacket(){
    rxMode=true;if(incoming.empty())return 0;
    rxCurrent=incoming.front();incoming.pop_front();rxPos=0;return rxCurrent.size();
  }
  int available(){return rxCurrent.size()-rxPos;}
  int read(){return available()?(unsigned char)rxCurrent[rxPos++]:-1;}
  size_t write(const uint8_t* data,size_t n){current.assign((const char*)data,n);return writeFail?0:n;}
  void onTxDone(void (*cb)()){txDone=cb;}
  void idle(){pending=false;txEnd=fakeMillis;}
  void tick(){if(pending&&fakeMillis>=txEnd){pending=false;if(txDone)txDone();}}
private:
  bool isTransmitting(){return fakeMillis<txEnd;}
public:
  int beginPacket(){if(isTransmitting())return 0;rxMode=false;current.clear();return 1;}
  size_t print(const char* s){current=s;return writeFail?0:current.size();}
  int endPacket(bool async){if(endFail||!async)return 0;packets.push_back(current);txEnd=fakeMillis+txDuration;pending=true;return 1;}
};
inline LoRaMock LoRa;
#endif
constexpr int FILE_WRITE=1,FILE_READ=2;
struct FileData {std::vector<uint8_t> bytes;};
inline bool failWrite=false,failRead=false,failSeek=false;
inline int writeCalls=0, failWriteCall=0, flushCount=0;
inline bool failFlush=false, failDirectory=false;
struct File {
  std::shared_ptr<FileData> data;
  size_t pos=0;
  bool active=false, dir=false;
  int mode=0;
  std::string pathName;
  std::vector<std::string> entries;
  File()=default;
  File(std::shared_ptr<FileData> d,int m,const std::string& path):data(d),active(true),mode(m),pathName(path){}
  explicit operator bool() const{return active;}
  bool isDirectory(){return active&&dir;}
  const char* name() const{auto i=pathName.find_last_of('/');return pathName.c_str()+(i==std::string::npos?0:i+1);}
  const char* path() const{return pathName.c_str();}
  size_t size() const{return active&&data?data->bytes.size():0;}
  size_t position() const{return pos;}
  void flush(){++flushCount;if(failFlush && active && data && !data->bytes.empty())data->bytes.pop_back();}
  void close(){active=false;}
  size_t write(const uint8_t* src,size_t n){
    if(!active||dir||mode!=FILE_WRITE)return 0;
    ++writeCalls;
    if(failWrite || writeCalls==failWriteCall)n/=2;
    if(data->bytes.size()<pos+n)data->bytes.resize(pos+n);
    memcpy(data->bytes.data()+pos,src,n);pos+=n;return n;
  }
  size_t read(uint8_t* dst,size_t n){
    if(!active||dir||mode!=FILE_READ||failRead||pos>data->bytes.size())return 0;
    n=std::min(n,data->bytes.size()-pos);
    if(n)memcpy(dst,data->bytes.data()+pos,n);
    pos+=n;return n;
  }
  bool seek(size_t p){if(!active||dir||failSeek||p>size())return false;pos=p;return true;}
  File openNextFile();
  std::string getNextFileName(bool* isDir){
    if(failDirectory){errno=EIO;return "";}
    if(!active||!dir||pos>=entries.size())return "";
    *isDir=false;return entries[pos++];
  }
};
struct LittleFSMock {
  bool ready=true,openFail=false,formatFlag=false,mkdirFail=false,removeFail=false,dirExists=false;
  unsigned int writeOpens=0;
  size_t capacity=1048576, otherUsed=0;
  std::string removeFailPath, openFailPath;
  std::map<std::string,std::shared_ptr<FileData>> files;
  std::shared_ptr<FileData> saved;
  size_t totalBytes(){return capacity;}
  size_t usedBytes(){size_t n=otherUsed;for(auto &it:files)n+=it.second->bytes.size();return n;}
  bool begin(bool format){formatFlag=format;if(!ready&&format)files.clear();return ready;}
  bool exists(const char* path){return ready&&((std::string(path)=="/ensaios"&&dirExists)||files.count(path));}
  bool mkdir(const char* path){if(!ready||mkdirFail||strcmp(path,"/ensaios"))return false;dirExists=true;return true;}
  bool remove(const char* path){if(!ready||removeFail||path==removeFailPath)return false;return files.erase(path)>0;}
  File open(const char* name,int mode=FILE_READ){
    if(!ready||openFail||name==openFailPath)return File();
    if(!strcmp(name,"/ensaios")){
      if(!dirExists)return File();
      File f;f.active=true;f.dir=true;f.pathName=name;
      for(auto&it:files)if(it.first.rfind("/ensaios/",0)==0)f.entries.push_back(it.first);
      return f;
    }
    if(mode==FILE_WRITE){saved=std::make_shared<FileData>();files[name]=saved;++writeOpens;}
    auto found=files.find(name);if(found==files.end())return File();
    return File(found->second,mode,name);
  }
};
inline LittleFSMock LittleFS;
inline File File::openNextFile(){
  if(!active||!dir)return File();
  while(pos<entries.size()){
    auto path=entries[pos++];auto f=LittleFS.open(path.c_str(),FILE_READ);if(f)return f;
  }
  return File();
}
