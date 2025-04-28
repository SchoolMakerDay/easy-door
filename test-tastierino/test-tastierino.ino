#define ROWS 4
#define COLS 3
#define R1 3
#define R2 8
#define R3 7
#define R4 5
#define C1 4
#define C2 2
#define C3 6
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
char curkey=NULL;
char oldkey=NULL;
char getKey(void);
void setup() {
  // put your setup code here, to run once:
  pinMode(R1,INPUT_PULLUP);
  pinMode(R2,INPUT_PULLUP);
  pinMode(R3,INPUT_PULLUP);
  pinMode(R4,INPUT_PULLUP);
  pinMode(C1,OUTPUT);
  pinMode(C2,OUTPUT);
  pinMode(C3,OUTPUT);
  digitalWrite(C1,HIGH);
  digitalWrite(C2,HIGH);
  digitalWrite(C3,HIGH);
  Serial.begin(9600);
}

void loop() {
  curkey=getKey();
  if (curkey!=NULL) Serial.println(curkey);
}

char getKey(void){
  int r1,r2,r3,r4;
  char c=NULL;
  //test C1
  digitalWrite(C1,LOW);
  digitalWrite(C2,HIGH);
  digitalWrite(C3,HIGH);
  r1=digitalRead(R1);
  r2=digitalRead(R2);
  r3=digitalRead(R3);
  r4=digitalRead(R4);
  
  if      (r1==LOW) {c=keys[0][0];}
  else if (r2==LOW) {c=keys[1][0]; }
  else if (r3==LOW) {c=keys[2][0]; }
  else if (r4==LOW) {c=keys[3][0]; } 
  //test C2
  digitalWrite(C1,HIGH);
  digitalWrite(C2,LOW);
  digitalWrite(C3,HIGH);
  r1=digitalRead(R1);
  r2=digitalRead(R2);
  r3=digitalRead(R3);
  r4=digitalRead(R4);
  if      (r1==LOW) {c=keys[0][1]; }
  else if (r2==LOW) {c=keys[1][1]; }
  else if (r3==LOW) {c=keys[2][1]; }
  else if (r4==LOW) {c=keys[3][1]; } 
   //test C3
  digitalWrite(C1,HIGH);
  digitalWrite(C2,HIGH);
  digitalWrite(C3,LOW);
  r1=digitalRead(R1);
  r2=digitalRead(R2);
  r3=digitalRead(R3);
  r4=digitalRead(R4);
  if      (r1==LOW) {c=keys[0][2]; }
  else if (r2==LOW) {c=keys[1][2]; }
  else if (r3==LOW) {c=keys[2][2]; }
  else if (r4==LOW) {c=keys[3][2]; } 
  if(c==oldkey){
    c=NULL;
  }
  
  else {
    oldkey=c;
  }
  delay(1);
  return c;
}