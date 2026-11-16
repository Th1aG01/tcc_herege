#pragma once
#include <cassert>
// Modelo NOR em RAM: as operacoes passam pela API SPI usada pelo driver real.
struct SPIMock {
  std::vector<int> args;
  std::vector<uint8_t> bytes = std::vector<uint8_t>(2097152, 0xff);
  std::vector<uint8_t> dados;
  uint32_t jedec=0xef4015, addr=0, reads=0, programs=0, erases=0, transactions=0, radioTransactions=0;
  int selected=-1, pos=0;
  uint8_t cmd=0, protection=0, protection2=0;
  bool wel=false, absent=false, busy=false, inside=false;
  bool failProgram=false, failErase=false, failWel=false;
  int cutAfter=-1, mutation=0;
  bool cut=false;
  void begin(int a,int b,int c,int d){args={a,b,c,d};}
  void beginTransaction(const SPISettings&) {
    assert(!inside && !(pins[13]==LOW && pins[21]==LOW));
    inside=true;selected=-1;pos=0;cmd=0;addr=0;dados.clear();++transactions;
  }
  uint8_t transfer(uint8_t b) {
    assert(inside && !(pins[13]==LOW&&pins[21]==LOW));
    int cs=pins[13]==LOW?13:pins[21]==LOW?21:-1;
    assert(cs!=-1 && (selected==-1 || selected==cs));selected=cs;
    if(pos++==0){cmd=b;return 0;}
    if(cs==21){if(cmd==0x12)return radioFlags;if(cmd==0x92)radioFlags&=~b;return 0;}
    if(absent||cut)return 0xff;
    if(cmd==0x9f)return pos<=4?uint8_t(jedec>>(8*(4-pos))):0xff;
    if(cmd==0x05)return protection|(wel?2:0)|(busy?1:0);
    if(cmd==0x35)return protection2;
    if(cmd==0x03||cmd==0x02||cmd==0x20){
      if(pos<=4){addr=(addr<<8)|b;return 0;}
      if(cmd==0x03){assert(addr<bytes.size());++reads;return bytes[addr++];}
      if(cmd==0x02)dados.push_back(b);
    }
    return 0;
  }
  void endTransaction(){
    assert(inside&&pins[13]==HIGH);inside=false;
    if(selected==21)++radioTransactions;
    if(selected!=13||absent||cut)return;
    if(cmd==0x06){wel=!failWel;return;}
    if(cmd!=0x02&&cmd!=0x20)return;
    assert(wel&&!busy);wel=false;++mutation;
    bool tearing=cutAfter>=0&&mutation>=cutAfter;
    if(cmd==0x02){
      ++programs;assert(!dados.empty()&&dados.size()<=256&&(addr&255)+dados.size()<=256);
      assert(addr+dados.size()<=bytes.size());
      size_t n=tearing?dados.size()/2:dados.size();
      if(!failProgram)for(size_t i=0;i<n;i++)bytes[addr+i]&=dados[i];
    }else{
      ++erases;assert(!(addr&4095)&&addr+4096<=bytes.size());
      if(!failErase)std::fill(bytes.begin()+addr,bytes.begin()+addr+(tearing?2048:4096),0xff);
    }
    if(tearing)cut=true;
  }
};
inline SPIMock SPI;
