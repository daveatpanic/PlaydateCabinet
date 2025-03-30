#include <Wire.h>

bool i2cread(int addr, uint8_t* buf, int count)
{
  int n = Wire.requestFrom(addr, count);

  if ( n < count )
    return false;

  int i;

  for ( i = 0; i < count && Wire.available(); ++i )
  {
    buf[i] = Wire.read();
 //    Serial.print(buf[i], HEX);
 //    Serial.print(" ");
  }

  if ( i < count )
    return false;

  // Serial.println("");

  return true;
}

bool i2cwrite(int addr, uint8_t* buf, int count)
{
  bool ret;

  Wire.beginTransmission(addr);
  ret = (Wire.write(0x00) == 1 && Wire.write(buf, count) == count);
  Wire.endTransmission();

  return ret;
}


bool tryCrank(int addr)
{
  uint8_t buf[10];

  if ( !i2cread(addr, buf, 10) )
  {
    Serial.println("i2cread failed");
    return false;
  }

  buf[0] = (buf[7] & 0x78) | 0x01;
  buf[1] = buf[8];
  buf[2] = (buf[9] & 0x1f) | 0x40;

  if ( !i2cwrite(addr, buf, 3) )
  {
    Serial.println("write failed");
    return false;
  }
  
  if ( !i2cread(addr, buf, 10) )
  {
    Serial.println("i2cread #2 failed");
    return false;
  }

  return true;
}


#define CRANK1_POWER 3
#define CRANK2_POWER 2

#define CRANK1_ADDR 0x5e //if SDA high at boot
#define CRANK2_ADDR 0x1f //if SDA low at boot

void initCranks()
{
  // crank 1 is on xiao's 3v3 line, and SDA is on a pullup, so it'll have address 0x5e
  // then we set SDA low, turn on crank 2, and it should have address 0x1f

  pinMode(CRANK2_POWER, OUTPUT);
  digitalWrite(CRANK2_POWER, LOW);

  pinMode(4,OUTPUT);
  digitalWrite(4, LOW); // set SDA low

  delay(10);

  digitalWrite(CRANK2_POWER, HIGH); // turn on crank 2
  delay(1); // at least 200 uS for address set

  // set SDA and SCL to inputs
  pinMode(4,INPUT);
  pinMode(5,INPUT);

  pinMode(CRANK1_POWER,OUTPUT);
  digitalWrite(CRANK1_POWER, HIGH);

  delay(100);
  // init i2c
  Wire.begin();
  Serial.println("wire.begin() begun");

  Serial.println("looking for cranks..");
  
  if ( tryCrank(CRANK1_ADDR) )
    Serial.println("crank 1 (0x53) found");
  else
    Serial.println("crank 1 (0x53) not found :( :( :(");

  if ( tryCrank(CRANK2_ADDR) )
    Serial.println("crank 2 (0x1f) found");
  else
    Serial.println("crank 2 (0x1f) not found :( :( :(");
}

void setup()
{
  Serial1.begin(115200);
  Serial.begin(9600);
  delay(2000); // 

  initCranks();
}

struct crank
{
  int addr;
  float startangle;
  float lastangle;
  float xavg;
  float yavg;
  bool handlepresent;
};

struct crank crank1 = { .addr = CRANK1_ADDR, .startangle = -1 };
struct crank crank2 = { .addr = CRANK2_ADDR, .startangle = -1 };

#define CRANK_SMOOTH_THRESHOLD 10 // if x or y changes more than this amount, don't smooth value
#define CRANK_SMOOTHING 10 // but for smaller movement we need to average out the noise

// below this amount of angle change, system reports no change
#define CRANK_THRESHOLD 0.8f

// rate limit our reporting so we don't overwhelm the poor Pi
#define REPORT_PERIOD 50

bool sampleCrankSensor(struct crank* crank)
{
  int8_t buf[5];

#define ABS(x) (((x)>=0)?(x):-(x))
#define MAX(x,y) (((x)>(y))?(x):(y))

  if ( !i2cread(crank->addr, (uint8_t*)buf, 5) )
    return false;

  int y = ((int)buf[1] << 4) | (buf[4] & 0xf);
  int x = ((int)buf[0] << 4) | (((uint8_t)buf[4] >> 4) & 0xf);
  
  if ( ABS(y) <= 20 && ABS(x) <= 20 )
  {
    crank->handlepresent = false;
    return true;
  }

  if ( ABS(x-crank->xavg) < CRANK_SMOOTH_THRESHOLD && ABS(y-crank->yavg) < CRANK_SMOOTH_THRESHOLD )
  {
    crank->xavg += (x - crank->xavg) / CRANK_SMOOTHING;
    crank->yavg += (y - crank->yavg) / CRANK_SMOOTHING;
  }
  else
  {
    crank->xavg = x;
    crank->yavg = y;
  }

  crank->handlepresent = true;
  return true;
}

static int count = 0;


void updateCrank(struct crank* crank)
{
    if ( crank->handlepresent )
    {
      float angle = atan2(crank->yavg,-crank->xavg) * 180 / M_PI;

      if ( angle < 0 ) angle += 360;

      if ( crank->startangle == -1 )
      {
        // XXX - need to debounce this
        Serial.println("handle in");
        Serial1.println("in");
        crank->startangle = angle;
        crank->lastangle = 0;
      }

      angle -= crank->startangle;
      if ( angle < 0 ) angle += 360;

      float change = angle - crank->lastangle;
    
      if ( change > 180 )
        change -= 360;
      else if ( change < -180 )
        change += 360;

      if ( ABS(change) > CRANK_THRESHOLD )
      {
        Serial.println(angle, 1);
        Serial1.println(angle, 1);
        crank->lastangle = angle;
      }
    }
    else if ( crank->startangle != -1 )
    {
      Serial.println("handle out");
      Serial1.println("out");
      crank->startangle = -1;
    }
}

void loop()
{
  bool handlepresent;

  if ( !sampleCrankSensor(&crank1) ) Serial.println("crank 1 not found");
  if ( !sampleCrankSensor(&crank2) ) Serial.println("crank 2 not found");

  if ( ++count == REPORT_PERIOD )
  {
    count = 0;
    updateCrank(&crank1);
    updateCrank(&crank2);
  }
/*
  Serial.print(crank1.lastangle);
  Serial.print(" ");
  Serial.println(crank2.lastangle);
  */
}
