/*------------------------------------------
ペットボトル型バーサライター製作　2016.3.26
TUT-CC TeamFAH

ソフトウェア　藤野滉司
ハードウェア　林幹二
アドバイザー　佐藤之斗　重名英史

--------------------------------------------*/

/**
2017.4.5 ver.1.4.7
　・画像表示　AUTO、Keep、Rearveを実装
　・モータースピードの可変化、ロギングへの反映
　・周期の変数の名前を変更　Period→ObsPeriod、UsablePriod→Period
　・DebugModeの変数の型を変更
　・KeepTimeに閾値を追加
　・共有メモリの読み込み頻度を10秒おきから1秒おきに変更
　＊共有メモリによるモード反映は失敗
　＊メモリ不足による表示乱れ
*/

#include "Tlc5940.h"
#include <iSdio.h>
#include <utility/Sd2CardExt.h>
#include <SPI.h>

#define ReflectorPin 0
#define MotorPin 6   
#define ChipSelectPin 5
#define ImageNumber 3

Sd2CardExt card;

int TlcUpdateTime = 313; //MicroSec LED点灯にかかる時間

uint8_t PixData[512];
uint8_t buffer_sharedMem[12];
uint8_t send_sharedMem[8];

unsigned long OldTime, NewTime, ChangeTime,ThisTime, ElapsedTime, KeepTime, ObsPeriod, Period;
int ImageCount;

boolean Interruption = false;
boolean DebugMode;

int Brightness[7], MotorOutput;
byte BrtMode, DspMode, KeepMode;

void setup() {
  //-----------------------------------------------------------初期値設定
  //Period = 70000;
  KeepTime = 100;
  MotorOutput = 215;
  ImageCount = 0;
  
  Brightness[0] = 0; 
  Brightness[1] = 1000;
  Brightness[2] = 2000;
  Brightness[3] = 3000;
  Brightness[4] = 4000;
  Brightness[5] = 4000; //Alternative
  Brightness[6] = 0; //TripleFlash
  
  BrtMode = 3;
  DspMode = 1;  //0: static, 1: rotation, 2:debug
  KeepMode = 0; //0: 画像変更なし, 1: 画像変更あり
  DebugMode = false;
  
  
  //-----------------------------------------------------------モーター回転開始
  
  analogWrite(MotorPin,MotorOutput);
  
  //-----------------------------------------------------------TLC初期化
  Tlc.init();
  
  //----------------------------------------------------------FlashAir初期化
  card.init(SPI_HALF_SPEED, ChipSelectPin);
  
  //-----------------------------------------------------------割り込みピン0有効化
  attachInterrupt(ReflectorPin,Frag,RISING);
  
    
  
  
  //-----------------------------------------------------------加速中表示
  for(int i=31; i > -1; i--){
    Tlc.set(i,Brightness[4]);
    Tlc.update();
    delay(300);
  }
  Tlc.clear();
  Tlc.update();
  
  //-----------------------------------------------------------周期測定
  Interruption = 0;
  while(!Interruption)
  OldTime = micros();
  Interruption = 0;
  while(!Interruption)
  NewTime = micros();
  Period = NewTime - OldTime;
  
  
  //-----------------------------------------------------------データ読み込み
  char CallFile[] = "Welcome.dat";
  LoadData(CallFile);
  ChangeTime = millis();
  
  if(DebugMode){Log(0);}
}


//-----------------------------------------------------------SDカードからのデータ読み込み
void LoadData(char FileName[]){ 
  File PixFile;
  SD.begin(ChipSelectPin);
  PixFile = SD.open(FileName);
  if (PixFile) {
    while (PixFile.available()) {PixFile.read(PixData,512);}
    PixFile.close();
  }
}

//-----------------------------------------------------------割り込み処理
void Frag(){
  if(DspMode == 2){
    Tlc.setAll(4000);
    Tlc.update();
  }else{
    Interruption = true;
  }
}


//-----------------------------------------------------------ディスプレイ:無回転
void Static(byte B){
  for(int i = 0; i < 512; i++){
    for(int j = 0; j < 8; j++){
      Tlc.set(i%4*8+j,((PixData[i]&(1<<j))>>j)*Brightness[B]);
    }
    if(Interruption){break;}
    if(i%4 == 3){Tlc.update();delayMicroseconds(KeepTime);}
  }
}

//-----------------------------------------------------------ディスプレイ:回転
int StartData = 512;
void Rotation(byte B){
  StartData += 8;
  if(StartData >= 512){StartData = 0;}
  for(int i = StartData; i < 512; i++){
   for(int j = 0; j < 8; j++){
     Tlc.set(i%4*8+j,((PixData[i]&(1<<j))>>j)*Brightness[B]);
   }
   if(Interruption){return;}
   if(i%4 == 3){Tlc.update();delayMicroseconds(KeepTime);}
  }
  
  for(int i = 0; i < 512; i++){
   for(int j = 0; j < 8; j++){
     Tlc.set(i%4*8+j,((PixData[i]&(1<<j))>>j)*Brightness[B]);
   }
   if(Interruption){return;}
   if(i%4 == 3){Tlc.update();delayMicroseconds(KeepTime);}
  }
}


//-----------------------------------------------------------点灯時間計算
void UpdateKeepTime(){
  NewTime = micros();
  ObsPeriod = NewTime - OldTime;
  if(ObsPeriod < 1.8*Period && ObsPeriod > 0.6*Period){Period = ObsPeriod;}
  KeepTime = Period/128-TlcUpdateTime;
  Interruption = false;
}

//-----------------------------------------------------------表示画像変更
void ImageChange(){
  ImageCount++;
  if(ImageCount == 1){char CallFile[] = "Welcome.dat";LoadData(CallFile);}
  if(ImageCount == 2){char CallFile[] = "TUT.dat";LoadData(CallFile); }
  if(ImageCount == 3){char CallFile[] = "Block.dat";LoadData(CallFile); }
  if(ImageCount == ImageNumber){ImageCount = 0;}
}

//-----------------------------------------------------------パイプ表示
int Pa = 0;
int Pb = 10;
int Pc = 20;
int delaytime = 100;
long ChangeTimeP, ThisTimeP;
void Pipe(byte B){
  Tlc.setAll(0);
  Pa++;if(Pa > 32){Pa = 0;}
  Pb++;if(Pb > 32){Pb = 0;}
  Pc++;if(Pc > 32){Pc = 0;}
  Tlc.set(Pa,Brightness[B]);
  Tlc.set(Pa+1,Brightness[B]);
  Tlc.set(Pb,Brightness[B]);
  Tlc.set(Pb+1,Brightness[B]);
  Tlc.set(Pc,Brightness[B]);
  Tlc.set(Pc+1,Brightness[B]);
  Tlc.update();
  ThisTimeP = millis();
  if(ThisTimeP - ChangeTimeP > 3000){delaytime = 10;}
  if(ThisTimeP - ChangeTimeP > 3000){delaytime = 100;ChangeTimeP = ThisTimeP;}
  delay(delaytime);
}

//-----------------------------------------------------------共有メモリの読み取り
void ReadSharedMemory(){
  switch(buffer_sharedMem[0]){
    case 1:
      if(buffer_sharedMem[1] == 0){MotorOutput = 0;}
      if(buffer_sharedMem[1] == 1){MotorOutput -= 5;}
      if(buffer_sharedMem[1] == 2){MotorOutput += 5;}
      if(buffer_sharedMem[1] == 3){MotorOutput = 255;}
      analogWrite(MotorPin, MotorOutput);
      break;
    case 2:
      DspMode = buffer_sharedMem[2];
      break;
    case 3:
      if(buffer_sharedMem[3] == 0){KeepMode = 0;}
      if(buffer_sharedMem[3] == 1){ImageChange();ChangeTime = millis();}
      if(buffer_sharedMem[3] == 2){
        if(ImageCount < 1){ImageCount = 0;}else{ImageCount -= 2;}
        ImageChange();
        ChangeTime = millis();
      }
      if(buffer_sharedMem[3] == 3){KeepMode = 1;}
      if(buffer_sharedMem[3] == 4){
        KeepMode = 1;
        char CallFile[] = "Up.dat";
        LoadData(CallFile);
      }
      break;
    case 4:
      BrtMode = buffer_sharedMem[4];
  }
   send_sharedMem[0]=0x00;
   card.writeExtMemory(1,1,0x1000,0x01,send_sharedMem);
}


//-----------------------------------------------------------ロギング
void Log(byte s){
  SD.begin(ChipSelectPin);
  File LogFile = SD.open("Log.csv", FILE_WRITE);
  char con[] = ",";
  switch(s){
    case 0:
      LogFile.println("Start");
    case 1:
      LogFile.print(ThisTime);
      LogFile.print(con);
      LogFile.print(ChangeTime);
      LogFile.print(con);
      LogFile.print(OldTime);
      LogFile.print(con);
      LogFile.print(NewTime);
      LogFile.print(con);
      LogFile.print(MotorOutput);
      LogFile.print(con);
      LogFile.print(Period);
      LogFile.print(con);
      LogFile.print(ObsPeriod);
      LogFile.print(con);
      LogFile.print(KeepTime);
      LogFile.print(con);
      LogFile.print(DspMode);
      LogFile.print(con);
      LogFile.print(BrtMode);
      LogFile.print(con);
      LogFile.println(ImageCount);
      break;
  }
  LogFile.close();
}


//-----------------------------------------------------------ループ
int Counts;
int r = 0;
int d = -1;
byte FshCount = 0;
void loop() {
  ThisTime = millis();
  ElapsedTime = ThisTime- ChangeTime;
  if(ElapsedTime > 6000){
   if(!KeepMode){ImageChange();}
   ChangeTime = ThisTime;   
  }
  
  
   if(ElapsedTime > r * 1000 && ElapsedTime < (r + 1) * 1000){
     if(DebugMode){
       r++;
       if(r > 9){r = 0;}
       Log(1);
     }
     card.readExtMemory(1, 1, 0x1000, 0x08, buffer_sharedMem);
     if(buffer_sharedMem[0] > 0){ReadSharedMemory();}
   }
  
  if(BrtMode == 5){
    Brightness[5] += d * 200;
    if(Brightness[5] <= 0){Brightness[5] = 0;d = 1;}
    if(Brightness[5] >= 4000){Brightness[5] = 4000;d = -1;}
  }
  
  if(BrtMode == 6){
    FshCount++;
    switch(FshCount){
      case 1:
        Brightness[6] = 4095;
        break;
      case 2:
        Brightness[6] = 0;
        break;
      case 3:
        Brightness[6] = 4095;
        break;
      case 4:
        Brightness[6] = 0;
        break;
      case 5:
        Brightness[6] = 4095;
        break;
      case 6:
        Brightness[6] = 0;
        break;
    }
    if(FshCount == 15){FshCount = 0;}
  }
  
  OldTime = micros();
  switch(DspMode){
    case 0:
      Static(BrtMode);
      break;
    case 1:
      Rotation(BrtMode);
      break;
    case 2:
      Tlc.setAll(0);
      Tlc.update();
      break;
    }
  if(Interruption){UpdateKeepTime();}
}

