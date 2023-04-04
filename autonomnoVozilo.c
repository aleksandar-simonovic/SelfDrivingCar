//podesavanja za LCD displej
sbit LCD_RS at LATA0_bit;
sbit LCD_EN at LATA1_bit;
sbit LCD_D4 at LATB0_bit;
sbit LCD_D5 at LATB1_bit;
sbit LCD_D6 at LATB2_bit;
sbit LCD_D7 at LATB3_bit;

sbit LCD_RS_Direction at TRISA0_bit;
sbit LCD_EN_Direction at TRISA1_bit;
sbit LCD_D4_Direction at TRISB0_bit;
sbit LCD_D5_Direction at TRISB1_bit;
sbit LCD_D6_Direction at TRISB2_bit;
sbit LCD_D7_Direction at TRISB3_bit;

unsigned char strTxtPut[15];
unsigned char tekst[8];
int dist = 0;

int tunel=0, linija=1;
int sirina_tunela = 0;
static int enable;
unsigned pwm_period1, pwm_period2;
unsigned dutyLeft = 0;
unsigned dutyRight = 0;

float left_num = 0, right_num = 0;
unsigned prev_left = 0, prev_right = 0;

#define US_ENABLE 20
#define delay_start_line 25
#define delay_start_tunel 30

void pratiLiniju();
void pratiTunel();
void nastaviKretanje(int duty);
void ispisiPredjeniPut();
void racunajPut();


//prekidna rutina koja mjeri vrijeme trajanja echo signala ultrazvucnog senzora
void ultraSound() org 0x00003C             
{
     if(INTCON2bits.INT1EP == 0) //ako se prekidna rutina aktivira rastucom ivicom 
     {
          TMR1=0;  // ponistimo vrijednost tajmera
          T1CONbits.TON = 1; //aktiviramo tajmer
          INTCON2bits.INT1EP = 1; //promjenimo ivicu na koju se aktivira prekidna rutina (opadajuca)
     }
     else if(INTCON2bits.INT1EP == 1) //ako se prekidna rutina aktivira opadajucu ivicom 
     {
          T1CONbits.TON = 0; //deaktiviramo tajmer
          INTCON2bits.INT1EP = 0; //promjenimo ivicu na koju se aktivira prekidna rutina (rastuca)
     }
     IFS1BITS.INT1IF=0; //obrisemo interapt fleg 
}

//funkcija postavnja vrijednost faktora popune za PWM signale koji kontrolisu brzinu motora
void setDuty(int dutyL, int dutyR)
{
        dutyLeft = dutyL;
        dutyRight = dutyR;
        PWM_Set_Duty((pwm_period1*dutyRight)/10,2);
        PWM_Set_Duty((pwm_period2*dutyLeft)/10,3);
}

//prekidna rutina koja se aktivira nakon signala za pokretanja/zaustavljanje (sa daljinskog)
void IntDetectionReset() org 0x0014               
{
        enable = 1 - enable; //invertuje vrijednost (0 - zaustavi, 1 - pokreni)
        Lcd_Cmd(_LCD_CLEAR); //brise displej prije pokretanja
        IFS0.F0 = 0; //obrisemo interapt fleg
}

//funkcija koja se pokrece svakih 30ms i pobudjuje ultrazvucni senzor, 
//izracunava vrijednost koju je izmjerio (vraca 0 ako je greska u mjerenju)
void trigerUS() org 0x004A                     //Interrupt on TMR4
{

        if( INTCON2bits.INT1EP == 1 )
        {
                  TMR1=0;
     	}
     	
     	dist=((TMR1*8)/58); //dist - izmjerena udaljenost ili 0 za gresku pri mjerenju

		//sledeci dio koda se odnosi na kreiranje pravougaonog inpulsa trajanja 30us 
		//koji je potrebno dovesti na ulaz za pobudjivanje  ultrazvucnog senzorasenzora
      	T1CONbits.TON = 0;
        INTCON2bits.INT1EP = 0;
      	LATAbits.LATA4 = 0;
        Delay_us(2);
        LATAbits.LATA4 = 1;
        Delay_us(30);
        LATAbits.LATA4 = 0;

        IFS1bits.T4IF = 0; //obrisemo interapt fleg

}



void main()
{
        int a = 0;
		
		//postavljanje odgovarajucih ponova na ulazne i izlazne
        AD1PCFG = 0xFFFF;        // svi pinovi su digitalni
        TRISB = 0x0000;          
        TRISBbits.TRISB7 = 1;
        TRISBbits.TRISB12 = 1;
        TRISBbits.TRISB13 = 1;
        TRISBbits.TRISB14 = 1;
        TRISBbits.TRISB15 = 1;

        TRISAbits.TRISA4=0;
        TRISBbits.TRISB4=1;

		//podesvanja za tajmer 1
        T1CONbits.TCKPS=2;
        T1CONbits.TCS=0;
        T1CONbits.TSYNC=0;
        T1CONbits.TGATE=0;

        //podesvanja za tajmer 4 cija prekidna rutina pobudjuje ultrazvucni senzor
        T4CONbits.TON = 0;
        T4CONbits.T32 = 0;
        T4CONbits.TCS = 0;
        T4CONbits.TGATE = 0;
        T4CONbits.TCKPS = 2;
        PR4 = 3750;
        IEC1bits.T4IE = 1;
        IPC6bits.T4IP = 5;
        T4CONbits.TON = 1;
		
        //podesvanja za tajmer 5, koristi se prilikom mjerenja predjenog puta
        T5CONbits.TON = 0;
        T5CONbits.TCS = 0;
        T5CONbits.TGATE = 0;
        T5CONbits.TCKPS = 2;

        LATB = 0x0000;
        IPC0 = IPC0 | 0x0007; // prioritet INT0 = 3

        INTCON2 = 0; //prekidi aktivni na pozitivnu ivicu
        IFS0 = 0; //interrupt flag cleared  INT0
        IEC0 = IEC0 | 0x0001; //Interrupt is set on a rising edge at INT0

        //PODESAVANJE ZA UZ
        IEC1bits.INT1IE = 0;
        INTCON2bits.INT1EP = 0;
        IPC5bits.INT1IP = 0b001;
        IEC1bits.INT1IE = 1;

        Unlock_IOLOCK();
        PPS_Mapping_NoLock(5, _OUTPUT, _OC2);
        PPS_Mapping_NoLock(6, _OUTPUT, _OC3);
        PPS_Mapping_NoLock(4, _INPUT, _INT1);
        Lock_IOLOCK();

		//podesavanje PWM signalja za pokretanje motora
        pwm_period1 = PWM_Init(1500, 2, 1, 2);
        pwm_period2 = PWM_Init(1500, 3, 1, 3);

        PWM_Set_Duty((pwm_period1*dutyRight)/10,2);           // Set current duty for PWM1
        PWM_Set_Duty((pwm_period2*dutyLeft)/10,3);            // Set current duty for PWM2

        PWM_Start(2);
        PWM_Start(3);

		//podesavanje LCD displeja
        Lcd_Init();                        // Initialize LCD
        Lcd_Cmd(_LCD_CLEAR);               // Clear display
        Lcd_Cmd(_LCD_CURSOR_OFF);          // Cursor off

        while(1)
        {
				//ako je vozilo pokrenuto, racunamo predjeni put i provjeravamo koju funkciju obavljamo
				//pracenje linje ili prolazak kroz tunel
                if (enable == 1)
                {
                        a = 1;
                        racunajPut(); //racunanje predjenog puta puta
                        if (linija == 1)  //ako pratimo linuju
                        {

                        		if ((dist > 0) && dist <= US_ENABLE )  //provjerimo da li smo naisli na tunel
                                {
                                        tunel = 1;		//ako jesmo setujemo promjenljivu tunel
                                        linija = 0;		// i resetujemo promjenljivu linija
                                }
                                else		//u suprotnom nastavljamo pratiti liniju
								{
                                	pratiLiniju(); //funkcija kontorlu vozila prilikom pracenje linije
								}
                        }
                        if ( tunel == 1 ) //ako prolazimo kroz tunel
                        {

                                if (PORTBbits.RB13 == 0 || PORTBbits.RB12 == 0 ) //provjeravamo da je jedan od senzora detektovao liniju
                                {
										linija = 1;	//ako jeste setujemo promjenljivu linija
                                        tunel = 0;	// i resetujemo promjenljivu tunel
                                        		
          								sirina_tunela = 0; //ponistimo vrijednos sirine tunela(postavlja se prilikom svakog ulaska u tunel)
                                }
                                else
								{
                                	pratiTunel(); //funkcija kontorlu vozila prilikom prolaska kroz tunel
								}
                        }
                }
                else  if (enable == 0 && a == 1) //ako dobijemo signal za zaustavljanje vozila, zaustavimo ga i za
                {									//postavimo pocetne vrijesnosti sledece pokretanje
                        a=0;
                        sirina_tunela = 0;
                        tunel = 0;
                        linija = 1;
                        setDuty(0,0);			//zaustavljanje vozila (faktor popune PWM signala je 0)
                        ispisiPredjeniPut();	//funkcija koja ispisuje predjeni put na LCD displej
                }
        }
}

//funkcija koja upravlja sa vozilom dok prati linuju
void pratiLiniju()
{       int a=6;
        if(PORTBbits.RB13 == 0 && dutyRight != 0) //ako je desni senzor detektovao liniju  
        {
                //skerni desno
                setDuty(a,0);//zaustavi desni motor
        }
        else if(PORTBbits.RB12 == 0 && dutyLeft !=0)//ako je lijevi senzor detektovao liniju 
        {
                //skerni lijevo
                setDuty(0,a);//zaustavi lijevi motor 
        }
        else if(PORTBbits.RB13 == 1 && PORTBbits.RB12 == 1 && (dutyRight == 0 || dutyLeft == 0) )//ako su oba senzora na bijeloj podozi 
        {
                //kreci se pravo
                nastaviKretanje(a);//pokreni oba motora ili nastanjlja kretanje nakon skretanja
        }
}

//funkcija koja upravlja sa vozilom dok prolazi kroz tunel
void pratiTunel()
{
			int a=6;
			if (sirina_tunela == 0 ) //ako ulazimo u tunel
            {
                    sirina_tunela = dist; //zapamtimo vrijednost sirine tunela
                    dutyLeft =0;		
                    dutyRight = 0;
            }
            if (dist > 0 && dist <  sirina_tunela  && dutyLeft !=0) //ako je distanca manja od sirine tunela
            {
                    //skreni lijevo
                    setDuty(0,a); //iskljuci lijevi motor
            }
            else if (dist > sirina_tunela && dist <= US_ENABLE  && dutyRight != 0)//ako je distanca veca od sirine tunela
            {										
                    //skerni desno
                    setDuty(a,0);//iskljuci desni motor
            }
            else if( dist == sirina_tunela  && (dutyRight == 0 || dutyLeft == 0))
            {
                    //kreci se pravo
                    nastaviKretanje(a);//pokreni oba motora ili nastanjlja kretanje nakon skretanja
            }

}

//funkcija koja pokrene oba motora ili nastanjlja kretanje nakon skretanja
void nastaviKretanje(int duty)
{
		int a =duty ;
		if (dutyRight == 0 && dutyleft != 0) //ako smo izasli iz procesa skretanja u desno
        {
                setDuty(dutyLeft,9);	//postavnjamo faktor popune PWM signala za desni motor na 90%, a za lijevi ostaje nepromjenjen
                if (tunel == 1)			//u zavisnosti da li smo pratimo liniju ili prolazimo kroz tunel
                    {					//zadrzavamo ovako stanje odgovarajuci period u ms i nakon toga

                        Delay_ms(delay_start_tunel);	
                    }
                    else
                    {
                      Delay_ms(delay_start_line);
                    }
                setDuty(dutyLeft,a);//smanjujemo faktor popune na istu vrijednost kao za lijevi motor
        }
        else if(dutyRight != 0 && dutyleft == 0)// isto kao i u prethodnom slucaju samo se odnosi 
        {										//na skretanje proces nakon skretanja u lijevo
                setDuty(9,dutyRight);
                if (tunel == 1)
                    {

                        Delay_ms(delay_start_tunel);
                    }
                    else
                    {
                      Delay_ms(delay_start_line);
                    }

                setDuty(a,dutyRight);
        }
        else	//ako pokrecemo vozilo, postavljamo faktor popune za oba motora na 90% i nakon odgovarajuceg perioda u ms 
        {		//smanjujemo tu vrijednost na unaprijed definisanu
        		if (tunel == 1)	//prilikom ulaska u tunel vozilo se vec krece i nema potrebe za dodatnim pojacanjem starta
                    {

                        setDuty(a,a);
                    }
                    else
                    {
                     	setDuty(9,9);
                		Delay_ms(delay_start_line);
                		setDuty(a,a);
                    }

        }
}

//funkcija za racunanje predjenog puta
void racunajPut()
{
        if((TMR5 == 0 || TMR5 >= 7500) && PORTBbits.RB14 == 1 && prev_right == 0)//ako je na senzorau visok logicki nivo i 
        {																		//nije slucajna (desila se u ocekivano vrijeme)		
        		T5CONbits.TON = 0;	//ugasimo tajmer 5
                right_num=right_num+1;	//povecamo vrijednost odmjeraka za 1
                prev_right = 1;			//oznacimo da se detektovali visok logicki novo
                T5CONbits.TON = 1;		//ponovno pokrenoemo tajmer 5
        }
        else if(PORTBbits.RB14 == 0)	//ako je na senzorau nizak logicki nivo
        {
                prev_right = 0;			//oznacimo da se detektovali nizak logicki novo
        }
        if(PORTBbits.RB15 == 1 && prev_left == 0)
        {
                left_num=left_num+1 ;
                prev_left = 1;
        }
        else if(PORTBbits.RB15 == 0)
        {
                prev_left = 0;
        }
}

//funkcija za ispis predjenog puta na LCD displej
void ispisiPredjeniPut()
{
        //float predjeniPut = ((right_num+left_num/6)*21.5)/2;
        float predjeniPut = (right_num/9)*20; //racunanje predjenog putna na osnovu broja odmjeraka sa senzora
        FloatToStr(predjeniPut, strTxtPut); //float vrijednost konvertujemo u string
        strncpy(tekst,strTxtPut,6);
        Lcd_Cmd(_LCD_CLEAR);

        Lcd_Out(1,1,tekst);		//ispis brojne vrijednosti, izrazene u centimetrima
        Lcd_Out(1,7,"cm");
        right_num = 0;			//postavnjane pocetne vrijednosti za sledece mjerenje
        left_num = 0;
}