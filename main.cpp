#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <string>
#include <bitset>
#include <map>
#include <unistd.h>

#define RED      "\033[1;31m"
#define GREEN    "\033[0;32m"
#define RST      "\e[0m"
#define TALKERID "AI"

const int HEADER_POSITION=0;
const int HAS_SENTENCES_POSITION=1;
const int MESSAGE_NUMBER_POSITION=2;
const int MESSAGE_ID_POSITION=3;
const int RADIO_CHANNEL_POSITION=4;
const int PAYLOAD_POSITION=5;
const int AIS_CHAR_BIT_LEN=6;
const int SHRT_MSG_BIT_LEN=167;
const int LONG_MSG_BIT_LEN=383;


std::vector<std::string> fragmentedMessage;
std::string NMEASentence="";
std::bitset<SHRT_MSG_BIT_LEN>bitContainerShort;
std::bitset<LONG_MSG_BIT_LEN>bitContainerLong;
std::bitset<AIS_CHAR_BIT_LEN>SixBitContainer;
std::string TalkerID="";
char fragmentIndicator;
char messageIndicator;
int bitCnt=0;

std::vector<std::string> aisVec;
std::map<long, decltype(aisVec)> aisMap;

/// @todo : gitHub version control function will be tested...


struct NMEA{
char  MessageType;
unsigned long	MMSI;
std::string     Environment;
double			Speed;
long			Longtitude;
long			Latitude;
float           radianLat;
float           radianLong;
unsigned int	Altitude;
unsigned int    timeStamp;
double			GHeading;
double			THeading;
char            Channel;
std::string		CallSign;
std::string 	ShipName;
std::string		Country;
}decodedPayload;

std::string ConvertToLongtitude(long longtitude){
    std::ostringstream returnVal;

    if( longtitude & 0x8000000 )
    {
        longtitude = 0x10000000 - longtitude;
        longtitude *= -1;
    }

    double long_ddd = (int) (longtitude / 600000);
    long_ddd += (double) (longtitude - (long_ddd * 600000)) / 600000.0;
    long_ddd = (short) (longtitude / 600000);
    double long_min = abs((longtitude - (long_ddd * 600000))) / 10000.0;

    char v;
	if (long_ddd>0)
		v='E';
	else
		v='W';


    returnVal << long_ddd << " " << long_min << " " << v;

    return returnVal.str();
 }

std::string ConvertToLatitude(long latitude){
std::ostringstream returnVal;
	if( latitude & 0x4000000 )
    {
        latitude = 0x8000000 - latitude;
        latitude *= -1;
    }


    double lat_dd = (int) (latitude / 600000);
	lat_dd += (double) (latitude - (lat_dd * 600000)) / 600000.0;

	lat_dd = (short) (latitude / 600000);
    double lat_min = abs((latitude - (lat_dd * 600000))) / 10000.0;

    char v;
	if (lat_dd>0)
		v='N';
	else
		v='S';

    returnVal << lat_dd << " " << lat_min << " " << v;

    return  returnVal.str();
}

void printStruct(const struct NMEA* decodedPayload){
    std::cout << "MsgType  : "  << decodedPayload->MessageType   << std::endl
              << "MMSI     : "  << decodedPayload->MMSI          << std::endl
              << "Country  : "  << decodedPayload->Country       << std::endl
              << "ShipName : "  << decodedPayload->ShipName      << std::endl
              << "Callsign : "  << decodedPayload->CallSign      << std::endl
              << "Environ  : "  << decodedPayload->Environment   << std::endl
              << "Altitude : "  << decodedPayload->Altitude      << std::endl
              << "Speed    : "  << decodedPayload->Speed         << std::endl
              << "GHeading : "  << decodedPayload->GHeading      << std::endl
              << "DeciLat  : "  << decodedPayload->Latitude      << std::endl
              << "DeciLong : "  << decodedPayload->Longtitude    << std::endl
              << "RadianLat: "  << decodedPayload->radianLat     << std::endl
              << "RadianLon: "  << decodedPayload->radianLong    << std::endl
              << "Latitude : " << ConvertToLatitude(decodedPayload->Latitude) << std::endl
              << "Longitude: " << ConvertToLongtitude(decodedPayload->Longtitude) << std::endl
              << "Time Stmp: "  << decodedPayload->timeStamp     << std::endl
              << "Channel  : "  << decodedPayload->Channel       << std::endl;

}

void initializeContainers(struct NMEA* decodedPayload,
                          std::vector<std::string>* fragmentedMessage,
                          std::bitset<SHRT_MSG_BIT_LEN>* bitContainerShort,
                          std::bitset<LONG_MSG_BIT_LEN>* bitContainerLong,
                          std::bitset<AIS_CHAR_BIT_LEN>* SixBitContainer){

    fragmentedMessage->clear();
    bitContainerLong->set(0);
    bitContainerShort->set(0);
    SixBitContainer->set(0);

    decodedPayload->MessageType  = 0;
    decodedPayload->MMSI         = 0;
    decodedPayload->Environment  = "";
    decodedPayload->Speed        = 0;
    decodedPayload->Longtitude   = 0;
    decodedPayload->Latitude     = 0;
    decodedPayload->radianLat    = 0.0;
    decodedPayload->radianLong   = 0.0;
    decodedPayload->Altitude     = 0;
    decodedPayload->GHeading     = 0;
    decodedPayload->THeading     = 0;
    decodedPayload->timeStamp    = 0;
    decodedPayload->CallSign     = "";
    decodedPayload->ShipName     = "";
    decodedPayload->Country      = "";
    decodedPayload->Channel      = '-';
}

std::string expandDoubleCommas(std::string message){

    std::string temp = "";

    std::size_t lastDoubleCommaPos = 0;
    while (message.find(",,", lastDoubleCommaPos) != std::string::npos) {
        std::size_t currentDoubleCommaPos =  message.find(",,",lastDoubleCommaPos);
        temp = temp + message.substr(lastDoubleCommaPos, currentDoubleCommaPos + 1) + " ";
        lastDoubleCommaPos = currentDoubleCommaPos + 1;
    }

    temp = temp + message.substr(lastDoubleCommaPos, message.length() - lastDoubleCommaPos);

    return temp;
}

void fragmentMessage(std::string* sentence){
    std::string streamedMsg= expandDoubleCommas(*sentence);
    std::string part ="";
    int len= streamedMsg.length();

        for (int j = 0, k = 0; j < len; j++) {
        if (streamedMsg[j] == ',') {
            std::string ch = streamedMsg.substr(k, j - k);
            k = j+1;
            fragmentedMessage.push_back(ch);
        }
        if (j == len - 1) {
            std::string ch = streamedMsg.substr(k, j - k+1);
            fragmentedMessage.push_back(ch);
        }
    }
    ///DEBUGGING PURPOSES... WILL BE REMOVED LATER ON...
   //std::vector<std::string>::iterator it= fragmentedMessage.begin();
   // for (; it !=fragmentedMessage.end(); it++)
   //     std::cout << *it << std::endl;
}

bool isRelevant(char* messageIndicator){
    TalkerID = fragmentedMessage[HEADER_POSITION].substr(1,2);
    fragmentIndicator = fragmentedMessage[MESSAGE_NUMBER_POSITION][0];
    *messageIndicator  = fragmentedMessage[PAYLOAD_POSITION][0];
    if (fragmentIndicator == '1')
    {
        if ((*messageIndicator == '1'  || *messageIndicator == '2' || \
             *messageIndicator == '3'  || *messageIndicator == '5' || \
             *messageIndicator == '9') && TalkerID == TALKERID)
            return true;
        else
            return false;
    }
    else
        return false;
}

bool isChecksumValid(std::string* NMEASentence){

	std::istringstream iSentence(*NMEASentence);
	std::string partToCalculate;
	std::string received_checksum;

	getline(iSentence, partToCalculate, '*');
	getline(iSentence, received_checksum);

    int len = partToCalculate.length();

    char check = partToCalculate[1];
    for (int i = 2; i < len ; i++)
    {
        if (partToCalculate[i] != ' ')
        {
            check ^=partToCalculate[i];
        }
    }

	std::ostringstream calulatedStream;
	calulatedStream << std::hex <<(int)check;
	std::string cSumResult = calulatedStream.str();

    for (unsigned int i = 0; i < received_checksum.length()-1 ; i++)
       if(!islower(received_checksum[i]))
            received_checksum[i] = tolower(received_checksum[i]);

    if (cSumResult.compare(received_checksum))
    {
		std::cout << "Message  : [ " GREEN "VALID" RST " ]" << std::endl;
		return true;
	}
	std::cout << "Message  : [ " RED "NOT VALID" RST " ]" << std::endl;
    return false;

}

unsigned char convertAisCharToSixBitChar(char *ch){
    unsigned char asciiValue = (unsigned char)*ch - 48;
    if ( asciiValue > 40)
        asciiValue -= 8;

    return asciiValue;
}

void binarizePayload(char* messageIndicator){
    std::string payLoad = fragmentedMessage[PAYLOAD_POSITION];

    if (*messageIndicator == '5')
    {
        bitCnt=LONG_MSG_BIT_LEN;
        for(unsigned i = 0; i < payLoad.length(); i++)
        {
            /// HERE WE CAN HAVE A PROBLEM... WE WILL CHECK!...
            unsigned char asciiValue = convertAisCharToSixBitChar(&payLoad[i]);

            SixBitContainer = std::bitset<AIS_CHAR_BIT_LEN>(asciiValue);

            for (int j = 5; j >= 0; j--)
            {
                bitContainerLong[bitCnt]=SixBitContainer[j];
                bitCnt--;
            }
        }

    }
    else
    {
        bitCnt=SHRT_MSG_BIT_LEN;
        for(unsigned i = 0; i < payLoad.length(); i++)
        {
            /// HERE WE CAN HAVE A PROBLEM... WE WILL CHECK!...
            unsigned char asciiValue = convertAisCharToSixBitChar(&payLoad[i]);
            SixBitContainer = std::bitset<AIS_CHAR_BIT_LEN>(asciiValue);

            for (int j = 5; j >= 0; j--)
            {
                bitContainerShort[bitCnt]=SixBitContainer[j];
                bitCnt--;
            }
        }
    }
}

unsigned int convertBitsToDecimal(int bitPosition, int NumOfBits, char messageIndicator){
int result = 0;
    if (messageIndicator == '5')
    {
       	for (int j = 0; j<=NumOfBits ; j++)
        {
            result *= 2;
            result += bitContainerLong[bitPosition];
            bitPosition--;
        }
    }
    else
    {
        for (int j = 0; j<=NumOfBits ; j++)
        {
            result *= 2;
            result += bitContainerShort[bitPosition];
            bitPosition--;
        }
    }
    return result;
}

char ais2ascii(char value){
     value = value & 0x3F;
     if( value < 0x20 )
         return value + 0x40;
     else
         return value;
}

std::string convertBitsToString(int bitPosition,
                                int NumOfBits,
                                char messageIndicator){
unsigned int convertedVal=0;
std::string returnVal;
std::ostringstream streamedText;

    for (int i = 0; i<NumOfBits ; i= i + AIS_CHAR_BIT_LEN)
    {
        for (int j = 0; j < AIS_CHAR_BIT_LEN; j++)
        {
            convertedVal *= 2;
            if (messageIndicator == '5')
                convertedVal += bitContainerLong[bitPosition-(i+j)];
            else
                convertedVal += bitContainerShort[bitPosition-(i+j)];
        }

        if (ais2ascii(convertedVal) == '@')
            streamedText<<"";
        else
            streamedText<<ais2ascii(convertedVal);
        convertedVal = 0;
    }
returnVal = streamedText.str();
    return returnVal;
}

std::string GetTalkerID(std::string TalkerID){
    if (TalkerID == "AB") TalkerID="NMEA 4.0 Base AIS station";
    if (TalkerID == "AD") TalkerID="NMEA 4.0 Dependent AIS station";
    if (TalkerID == "AI") TalkerID="Mobile AIS station";
    if (TalkerID == "AN") TalkerID="NMEA 4.0 Aid to Navigation AIS station";
    if (TalkerID == "AR") TalkerID="NMEA 4.0 AIS Receiving station";
    if (TalkerID == "AS") TalkerID="NMEA 4.0 Limited Base station";
    if (TalkerID == "AT") TalkerID="NMEA 4.0 AIS Transmitting station";
    if (TalkerID == "AX") TalkerID="NMEA 4.0 Repeater AIS station";
    if (TalkerID == "BS") TalkerID="Base AIS station";
    if (TalkerID == "SA") TalkerID="Physical Shore AIS station";

    return TalkerID;
}

std::string GetCountryName(short int countryCode){
    std::string countryName="";
    switch(countryCode)
    {
    case 500:
        countryName	= "Adélie Land";        break;
    case 401:
        countryName	= "Afghanistan";        break;
    case 303:
        countryName	= "Alaska";             break;
    case 201:
        countryName	= "Albania";            break;
    case 605:
        countryName	= "Algeria";            break;
    case 559:
        countryName	= "American Samoa";     break;
    case 202:
        countryName	= "Andorra" ;           break;
    case 603:
        countryName	= "Angola";             break;
    case 301:
        countryName	= "Anguilla";           break;
    case 304:
    case 305:
        countryName	= "Antigua and Barbuda";break;
    case 701:
        countryName	= "Argentine Republic"; break;
    case 216:
        countryName	= "Armenia";            break;
    case 307:
        countryName	= "Aruba";              break;
    case 608:
        countryName	= "Ascension Island";   break;
    case 503:
        countryName	= "Australia";          break;
    case 203:
        countryName	= "Austria";            break;
    case 423:
        countryName	= "Azerbaijani Republic"; break;
    case 204:
        countryName	= "Azores";             break;
    case 308:
    case 309:
    case 311:
        countryName	= "Bahamas";            break;
    case 408:
        countryName	= "Bahrain";            break;
    case 405:
        countryName	= "Bangladesh";         break;
    case 314:
        countryName	= "Barbados";           break;
    case 206:
        countryName	= "Belarus";            break;
    case 205:
        countryName	= "Belgium";            break;
    case 312:
        countryName	= "Belize";             break;
    case 610:
        countryName	= "Benin";              break;
    case 310:
        countryName	= "Bermuda";            break;
    case 410:
        countryName	= "Bhutan";             break;
    case 720:
        countryName	= "Bolivia";            break;
    case 306:
        countryName	= "Bonaire";            break;
    case 478:
        countryName	= "Bosnia";             break;
    case 611:
        countryName	= "Botswana";           break;
    case 710:
        countryName	= "Brazil";             break;
    case 378:
        countryName	= "British Virgin Islands"; break;
    case 508:
        countryName	= "Brunei Darussalam";  break;
    case 207:
        countryName	= "Bulgaria";           break;
    case 633:
        countryName	= "Burkina Faso";       break;
    case 609:
        countryName	= "Burundi";            break;
    case 514:
    case 515:
        countryName	= "Cambodia";           break;
    case 613:
        countryName	= "Cameroon";           break;
    case 316:
        countryName	= "Canada";             break;
    case 617:
        countryName	= "Cape Verde" ;        break;
    case 319:
        countryName	= "Cayman Islands";     break;
    case 612:
        countryName	= "Central African Republic"; break;
    case 670:
        countryName	= "Chad" ;              break;
    case 725:
        countryName	= "Chile";              break;
    case 412:
    case 413:
    case 414:
        countryName	= "China";              break;
    case 516:
        countryName	= "Christmas Island";   break;
    case 523:
        countryName	= "Cocos";              break;
    case 730:
        countryName	= "Colombia";           break;
    case 616:
    case 620:
        countryName	= "Comoros";            break;
    case 615:
        countryName	= "Congo" ;             break;
    case 518:
        countryName	= "Cook Islands";       break;
    case 321:
        countryName	= "Costa Rica";         break;
    case 619:
        countryName	= "Côte d'Ivoire";      break;
    case 238:
        countryName	= "Croatia";            break;
    case 618:
        countryName	= "Crozet Archipelago"; break;
    case 323:
        countryName	= "Cuba";               break;
    case 209:
    case 210:
    case 212:
        countryName	= "Cyprus";             break;
    case 270:
        countryName	= "Czech" ;             break;
    case 445:
        countryName	= "Korea";              break;
    case 676:
        countryName	= "Congo";              break;
    case 219:
    case 220:
        countryName	= "Denmark";            break;
    case 621:
        countryName	= "Djibouti";           break;
    case 325:
        countryName	= "Dominica" ;          break;
    case 327:
        countryName	= "Dominican" ;         break;
    case 735:
        countryName	= "Ecuador";            break;
    case 622:
        countryName	= "Egypt" ;             break;
    case 359:
        countryName	= "El Salvador" ;       break;
    case 631:
        countryName	= "Equatorial Guinea";  break;
    case 625:
        countryName	= "Eritrea";            break;
    case 276:
        countryName	= "Estonia";            break;
    case 624:
        countryName	= "Ethiopia";           break;
    case 740:
        countryName	= "Falkland Islands";   break;
    case 231:
        countryName	= "Faroe Islands";      break;
    case 520:
        countryName	= "Fiji";               break;
    case 230:
        countryName	= "Finland";            break;
    case 226:
    case 227:
    case 228:
        countryName	= "France";             break;
    case 546:
        countryName	= "French Polynesia";   break;
    case 626:
        countryName	= "Gabonese";           break;
    case 629:
        countryName	= "Gambia";             break;
    case 213:
        countryName	= "Georgia";            break;
    case 211:
    case 218:
        countryName	= "Germany";            break;
    case 627:
        countryName	= "Ghana";              break;
    case 236:
        countryName	= "Gibraltar";          break;
    case 237:
    case 239:
    case 240:
    case 241:
        countryName	= "Greece";             break;
    case 331:
        countryName	= "Greenland";          break;
    case 330:
        countryName	= "Grenada";            break;
    case 329:
        countryName	= "Guadeloupe";         break;
    case 332:
        countryName	= "Guatemala";          break;
    case 745:
        countryName	= "Guiana";             break;
    case 632:
        countryName	= "Guinea";             break;
    case 630:
        countryName	= "Guinea";             break;
    case 750:
        countryName	= "Guyana";             break;
    case 336:
        countryName	= "Haiti" ;             break;
    case 334:
        countryName	= "Honduras";           break;
    case 477:
        countryName	= "Hong Kong";          break;
    case 243:
        countryName	= "Hungary" ;           break;
    case 251:
        countryName	= "Iceland";            break;
    case 419:
        countryName	= "India";              break;
    case 525:
        countryName	= "Indonesia";          break;
    case 422:
        countryName	= "Iran";               break;
    case 425:
        countryName	= "Iraq";               break;
    case 250:
        countryName	= "Ireland";            break;
    case 428:
        countryName	= "Israel" ;            break;
    case 247:
        countryName	= "Italy";              break;
    case 339:
        countryName	= "Jamaica";            break;
    case 431:
    case 432:
        countryName	= "Japan";              break;
    case 438:
        countryName	= "Jordan";             break;
    case 436:
        countryName	= "Kazakhstan";         break;
    case 634:
        countryName	= "Kenya";              break;
    case 635:
        countryName	= "Kerguelen";          break;
    case 529:
        countryName	= "Kiribati";           break;
    case 440:
    case 441:
        countryName	= "Korea";              break;
    case 447:
        countryName	= "Kuwait";             break;
    case 451:
        countryName	= "Kyrgyzstan";         break;
    case 531:
        countryName	= "Lao People";         break;
    case 275:
        countryName	= "Latvia";             break;
    case 450:
        countryName	= "Lebanon";            break;
    case 644:
        countryName	= "Lesotho";            break;
    case 636:
    case 637:
        countryName	= "Liberia";            break;
    case 642:
        countryName	= "Libya";              break;
    case 252:
        countryName	= "Liechtenstein";      break;
    case 277:
        countryName	= "Lithuania";          break;
    case 253:
        countryName	= "Luxembourg";         break;
    case 453:
        countryName	= "Macao";              break;
    case 274:
        countryName	= "Macedonia";          break;
    case 647:
        countryName	= "Madagascar";         break;
    case 255:
        countryName	= "Madeira";            break;
    case 655:
        countryName	= "Malawi";             break;
    case 533:
        countryName	= "Malaysia";           break;
    case 455:
        countryName	= "Maldives";           break;
    case 649:
        countryName	= "Mali";               break;
    case 215:
    case 229:
    case 248:
    case 249:
    case 256:
        countryName	= "Malta";              break;
    case 538:
        countryName	= "Marshall";           break;
    case 347:
        countryName	= "Martinique";         break;
    case 654:
        countryName	= "Mauritania" ;        break;
    case 645:
        countryName	= "Mauritius";          break;
    case 345:
        countryName	= "Mexico";             break;
    case 510:
        countryName	= "Micronesia";         break;
    case 214:
        countryName	= "Moldova";            break;
    case 254:
        countryName	= "Monaco";             break;
    case 457:
        countryName	= "Mongolia";           break;
    case 262:
        countryName	= "Montenegro";         break;
    case 348:
        countryName	= "Montserrat";         break;
    case 242:
        countryName	= "Morocco";            break;
    case 650:
        countryName	= "Mozambique";         break;
    case 506:
        countryName	= "Myanmar";            break;
    case 659:
        countryName	= "Namibia";            break;
    case 544:
        countryName	= "Nauru";              break;
    case 459:
        countryName	= "Nepal";              break;
    case 244:
    case 245:
    case 246:
        countryName	= "Netherlands";        break;
    case 540:
        countryName	= "New Caledonia";      break;
    case 512:
        countryName	= "New Zealand";        break;
    case 350:
        countryName	= "Nicaragua";          break;
    case 656:
        countryName	= "Niger";              break;
    case 657:
        countryName	= "Nigeria";            break;
    case 542:
        countryName	= "Niue";               break;
    case 536:
        countryName	= "Northern Mariana";   break;
    case 257:
    case 258:
    case 259:
        countryName	= "Norway";             break;
    case 461:
        countryName	= "Oman" ;              break;
    case 511:
        countryName	= "Palau";              break;
    case 463:
        countryName	= "Pakistan" ;          break;
    case 443:
        countryName	= "Palestinian";        break;
    case 351:
    case 352:
    case 353:
    case 354:
    case 355:
    case 356:
    case 357:
    case 370:
    case 371:
    case 372:
    case 373:
    case 374:
        countryName	= "Panama";             break;
    case 553:
        countryName	= "Papua New Guinea";   break;
    case 755:
        countryName	= "Paraguay";           break;
    case 760:
        countryName	= "Peru";               break;
    case 548:
        countryName	= "Philippines";        break;
    case 555:
        countryName	= "Pitcairn Island";    break;
    case 261:
        countryName	= "Poland" ;            break;
    case 263:
        countryName	= "Portugal";           break;
    case 358:
        countryName	= "Puerto Rico";        break;
    case 466:
        countryName	= "Qatar";              break;
    case 660:
        countryName	= "Réunion";            break;
    case 264:
        countryName	= "Romania";            break;
    case 273:
        countryName	= "Russian Federation"; break;
    case 661:
        countryName	= "Rwandese";           break;
    case 665:
        countryName	= "Saint Helena";       break;
    case 341:
        countryName	= "Saint Kitts";        break;
    case 343:
        countryName	= "Saint Lucia";        break;
    case 607:
        countryName	= "Saint Paul";         break;
    case 361:
        countryName	= "Saint Pierre";       break;
    case 375:
    case 376:
    case 377:
        countryName	= "Saint Vincent";      break;
    case 561:
        countryName	= "Samoa" ;             break;
    case 268:
        countryName	= "San Marino";         break;
    case 668:
        countryName	= "São Tomé and Príncipe"; break;
    case 403:
        countryName	= "Saudi Arabia";       break;
    case 663:
        countryName	= "Senegal";            break;
    case 279:
        countryName	= "Serbia";             break;
    case 664:
        countryName	= "Seychelles";         break;
    case 667:
        countryName	= "Sierra Leone";       break;
    case 563:
    case 564:
    case 565:
    case 566:
        countryName	= "Singapore";          break;
    case 267:
        countryName	= "Slovakia";           break;
    case 278:
        countryName	= "Slovenia" ;          break;
    case 557:
        countryName	= "Solomon Islands";    break;
    case 666:
        countryName	= "Somalia";            break;
    case 601:
        countryName	= "South Africa";       break;
    case 224:
    case 225:
        countryName	= "Spain";              break;
    case 417:
        countryName	= "Sri Lanka";          break;
    case 638:
        countryName	= "South Sudan";        break;
    case 662:
        countryName	= "Sudan" ;             break;
    case 765:
        countryName	= "Suriname" ;          break;
    case 669:
        countryName	= "Swaziland";          break;
    case 265:
    case 266:
        countryName	= "Sweden";             break;
    case 269:
        countryName	= "Switzerland" ;       break;
    case 468:
        countryName	= "Syrian";             break;
    case 416:
        countryName	= "Taiwan" ;            break;
    case 472:
        countryName	= "Tajikistan" ;        break;
    case 674:
    case 677:
        countryName	= "Tanzania";           break;
    case 567:
        countryName	= "Thailand";           break;
    case 671:
        countryName	= "Togolese" ;          break;
    case 570:
        countryName	= "Tonga";              break;
    case 362:
        countryName	= "Trinidad";           break;
    case 672:
        countryName	= "Tunisia";            break;
    case 271:
        countryName	= "Turkey";             break;
    case 434:
        countryName	= "Turkmenistan";       break;
    case 364:
        countryName	= "Turks and Caicos";   break;
    case 572:
        countryName	= "Tuvalu";             break;
    case 675:
        countryName	= "Uganda";             break;
    case 272:
        countryName	= "Ukraine";            break;
    case 470:
    case 471:
        countryName	= "Arab Emirates";      break;
    case 232:
    case 233:
    case 234:
    case 235:
        countryName	= "Great Britain";      break;
    case 379:
        countryName	=	 "Virgin Islands";  break;
    case 338:
    case 366:
    case 367:
    case 368:
    case 369:
        countryName	= "America";            break;
    case 770:
        countryName	= "Uruguay";            break;
    case 437:
        countryName	= "Uzbekistan";         break;
    case 576:
    case 577:
        countryName	= "Vanuatu";            break;
    case 208:
        countryName	= "Vatican";            break;
    case 775:
        countryName	= "Venezuela";          break;
    case 574:
        countryName	= "Vietnam";            break;
    case 578:
        countryName	= "Wallis";             break;
    case 473:
    case 475:
        countryName	= "Yemen";              break;
    case 678:
        countryName	= "Zambia";             break;
    case 679:
        countryName	= "Zimbabwe";           break;
    default:
        countryName	= "--";
    }
    return countryName;

}

void fillStruct(struct NMEA* decodedPayload,
                const char *messageIndicator){

    if (*messageIndicator == '5')
    {
        decodedPayload->MMSI            = convertBitsToDecimal(375, 29 , *messageIndicator);
        decodedPayload->CallSign        = convertBitsToString(313 , 41 , *messageIndicator);
        decodedPayload->ShipName        = convertBitsToString(271 , 119, *messageIndicator);
        decodedPayload->Country         = GetCountryName(decodedPayload->MMSI/1000000);
    }
    else
    {

        if (*messageIndicator == '9')
        {
            decodedPayload->MMSI        = convertBitsToDecimal(159, 29, *messageIndicator);
            decodedPayload->Environment = "Air";
            decodedPayload->Country     = GetCountryName(decodedPayload->MMSI/1000000);
            decodedPayload->Altitude    = convertBitsToDecimal(129, 11, *messageIndicator)/3.28;
            decodedPayload->Speed       = convertBitsToDecimal(117, 9 , *messageIndicator)/10;
            decodedPayload->Longtitude  = convertBitsToDecimal(106, 27, *messageIndicator);
            decodedPayload->Latitude    = convertBitsToDecimal(78 , 26, *messageIndicator);
            decodedPayload->radianLat   = (float)((decodedPayload->Latitude/600000.0)*6.28)/360.0;
            decodedPayload->radianLong  = (float)((decodedPayload->Longtitude/600000.0)*6.28)/360.0;
            decodedPayload->GHeading    = convertBitsToDecimal(51 , 11, *messageIndicator)/10;
            decodedPayload->timeStamp   = convertBitsToDecimal(39 , 5 ,*messageIndicator);
        }
        else
        {
            decodedPayload->MMSI        = convertBitsToDecimal(159, 29, *messageIndicator);
            decodedPayload->Environment = "Surface";
            decodedPayload->Speed       = convertBitsToDecimal(117, 9 , *messageIndicator)/10;
            decodedPayload->Longtitude  = convertBitsToDecimal(106, 27, *messageIndicator);
            decodedPayload->Latitude    = convertBitsToDecimal(78 , 26, *messageIndicator);
            decodedPayload->radianLat   = (float)((decodedPayload->Latitude/600000.0)*6.28)/360.0;
            decodedPayload->radianLong  = (float)((decodedPayload->Longtitude/600000.0)*6.28)/360.0;
            decodedPayload->Country     = GetCountryName(decodedPayload->MMSI/1000000);
            decodedPayload->GHeading    = convertBitsToDecimal(51 , 11, *messageIndicator)/10;
            decodedPayload->THeading    = convertBitsToDecimal(39 , 8 , *messageIndicator);
            decodedPayload->timeStamp   = convertBitsToDecimal(39 , 5 ,*messageIndicator);
        }
    }
    if (decodedPayload->Speed == 511)
        decodedPayload->Speed = 0;
    decodedPayload->MessageType     = *messageIndicator;
    decodedPayload->Channel         = fragmentedMessage[RADIO_CHANNEL_POSITION][0];
}

bool is_fileexists(const std::string filename) {
    std::ifstream ifile(filename);
    return (bool)ifile;
}

const struct NMEA aisWrapper(struct NMEA* decodedPayload,
                             std::vector<std::string>* fragmentedMessage,
                             std::bitset<SHRT_MSG_BIT_LEN>* bitContainerShort,
                             std::bitset<LONG_MSG_BIT_LEN>* bitContainerLong,
                             std::bitset<AIS_CHAR_BIT_LEN>* SixBitContainer,
                             char *messageIndicator,
                             std::string *NMEASentence){

    struct NMEA structBody;

    initializeContainers(&structBody,
                         fragmentedMessage,
                         bitContainerShort,
                         bitContainerLong,
                         SixBitContainer);

    fragmentMessage(NMEASentence);

    if (isRelevant(messageIndicator))
    {
        if (isChecksumValid(NMEASentence))
        {
            binarizePayload(messageIndicator);
            fillStruct(&structBody,messageIndicator);
        }
    }

    return structBody;
}
///
/// This function seperates messages by MMSI Number
///
void seperateSentences(const struct NMEA* decodedMessage,
                       const std::string* sentence){

    long mms = decodedMessage->MMSI;
    if (mms != 0)
    {
        aisMap[mms].push_back(*sentence);
        std::cout << mms << " Added to mapper... " << std::endl;
    }
}
///
///This Function writes seperated MMSI messages to files
///And file names are given by the MMSI Number...
///
void printSeperatedLines(){
std::ofstream destinationFile;
    for (auto& it: aisMap)
    {
        std::string msgFile = std::to_string(it.first);

        int len = it.second.size();
        for (int i = 0; i < len; i++)
        {
            std::ofstream destinationFile(msgFile.c_str(),std::ios::out | std::ios::app);
            std::cout << i << "Writing to file : " << it.second[i] << std::endl;
            destinationFile << it.second[i] << std::endl;

        }
        destinationFile.close();
        usleep(80000);
    }
}

int main(int argc, char *argv[]){

    if (argc < 2)
    {
        std::cout << "File path/name must be provided..." << std::endl;
        exit(0);
    }
    struct NMEA decodedMessage;
    std::string fileName =argv[1];
    std::ifstream sourceFile;
    std::string fileAd;
    sourceFile.open(fileName);

    if (sourceFile.fail())
    {
        std::cout << "Couldn't open given file..." << std::endl;
        exit(1);
    }


    while(!sourceFile.eof())
    {
        NMEASentence = fileAd;
        getline(sourceFile, fileAd);

        decodedMessage = aisWrapper(&decodedPayload,
                                    &fragmentedMessage,
                                    &bitContainerShort,
                                    &bitContainerLong,
                                    &SixBitContainer,
                                    &messageIndicator,
                                    &fileAd);
        if (decodedMessage.MMSI > 0){
            printStruct(&decodedMessage);
        }
        usleep(600000);
    }

    return 0;
}




