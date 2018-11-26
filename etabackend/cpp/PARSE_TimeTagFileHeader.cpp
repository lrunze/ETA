/************************************************************************
Marcus Sackrow, PicoQuant_header_parser GmbH, December 2013
Michael Wahl, PicoQuant_header_parser GmbH, revised July 2014
Zuzeng Lin, KTH, Nov 2017
************************************************************************/

#ifdef __clang__
#define MKS_inline  __attribute__((always_inline))
#define VC_inline 
#else

#define VC_inline 
#define MKS_inline
#endif // _MSC_VER

#include "stdlib.h"
#include "stdio.h"
#include "fcntl.h" 
#include "string.h"
#include "time.h"
#include "io.h"
typedef wchar_t WCHAR;
long long order_gurantee = 0;
#define PERROR(...) {order_gurantee=printf( "\n [ERROR]"  __VA_ARGS__ );}
#define PINFO(...)  {order_gurantee=printf("\n" __VA_ARGS__);}

//global output registers for parsing time-tagged file
extern "C" {
	long long SYNCRate_pspr = 0;
	long long TTRes_pspr = 0;
	long long DTRes_pspr = 0;
	long long NumRecords = 0;
	long long RecordType = 0;
	long long BytesofRecords = 0;
	long long TTF_header_offset;
	long long TTF_filesize;

}
int MKS_inline bh_4bytes_header_parser(char Magic[4]) {
	PINFO("Becker & Hickl SPC-134/144/154/830 timetag file has no header.");
	SYNCRate_pspr = ((unsigned short *)Magic)[0];
	DTRes_pspr = 1;
	TTRes_pspr = 0;
	RecordType = 3;
	PINFO("RecordType: bh_spc_4bytes");
	BytesofRecords = 4;
	TTF_header_offset = 4;
	return 0;
}
int MKS_inline Swebian_header_parser() {
	PINFO("Swebian Instrument timetag file has no header.");
	SYNCRate_pspr = 0;
	TTRes_pspr = 1;
	DTRes_pspr = 1;
	RecordType = 1;
	PINFO("RecordType: SwebianInstrument 16-bytes");
	BytesofRecords = 16;
	TTF_header_offset=0;
	return 0;
}

int MKS_inline quTAU_FORMAT_BINARY_header_parser(FILE *fpin) {

	// read the rest 32 bytes of the header
	char Header[32];
	if (fread(&Header, 1, sizeof(Header), fpin) != sizeof(Header))
	{
		PERROR("Error when reading header, aborted.");
		return -1;
	}
	PINFO("quTAU_FORMAT_BINARY file header is read, but ignored.");
	RecordType = 0;
	BytesofRecords = 10;
	PINFO("RecordType: quTAU_FORMAT_BINARY 10-bytes");

	//set resolutions
	TTRes_pspr = 1; 
	DTRes_pspr = TTRes_pspr;
	SYNCRate_pspr = 1.249554 * 1000;

	// find size
	TTF_header_offset = ftell(fpin);

	return 0;
}
int MKS_inline quTAU_FORMAT_COMPRESSED_header_parser(FILE *fpin) {

	// read the rest 32 bytes of the header
	char Header[32];
	if (fread(&Header, 1, sizeof(Header), fpin) != sizeof(Header))
	{
		PERROR("Error when reading header, aborted.");
		return -1;
	}
	PINFO("quTAU_FORMAT_COMPRESSED file header is read, but ignored.");
	RecordType = 0;
	BytesofRecords = 5;
	PINFO("RecordType: quTAU_FORMAT_COMPRESSED 5-bytes");

	//set resolutions
	TTRes_pspr = 1; 
	DTRes_pspr = TTRes_pspr;
	SYNCRate_pspr = 1.249554 * 1000;

	// find size
	TTF_header_offset = ftell(fpin);

	return 0;
}



//#pragma pack(8) //structure alignment to 8 byte boundaries
// RecordTypes
#define rtPicoHarpT3     0x00010303    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $03 (T3), HW: $03 (PicoHarp)
#define rtPicoHarpT2     0x00010203    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T2), HW: $03 (PicoHarp)
#define rtHydraHarpT3    0x00010304    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $03 (T3), HW: $04 (HydraHarp)
#define rtHydraHarpT2    0x00010204    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T2), HW: $04 (HydraHarp)
#define rtHydraHarp2T3   0x01010304    // (SubID = $01 ,RecFmt: $01) (V2), T-Mode: $03 (T3), HW: $04 (HydraHarp)
#define rtHydraHarp2T2   0x01010204    // (SubID = $01 ,RecFmt: $01) (V2), T-Mode: $02 (T2), HW: $04 (HydraHarp)
#define rtTimeHarp260NT3 0x00010305    // (SubID = $00 ,RecFmt: $01) (V2), T-Mode: $03 (T3), HW: $05 (TimeHarp260N)
#define rtTimeHarp260NT2 0x00010205    // (SubID = $00 ,RecFmt: $01) (V2), T-Mode: $02 (T2), HW: $05 (TimeHarp260N)
#define rtTimeHarp260PT3 0x00010306    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T3), HW: $06 (TimeHarp260P)
#define rtTimeHarp260PT2 0x00010206    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T2), HW: $06 (TimeHarp260P)

// A Tag entry
struct TgHd {
	char Ident[32];     // Identifier of the tag
	int Idx;            // Index for multiple tags or -1
	unsigned int Typ;  // Type of tag ty..... see const section
	long long TagValue; // Value of tag.
} TagHead;


// TDateTime (in file) to time_t (standard C) conversion
const int EpochDiff = 25569; // days between 30/12/1899 and 01/01/1970
const int SecsInDay = 86400; // number of seconds in a day
time_t TDateTime_TimeT(double Convertee)
{
	time_t Result((long)(((Convertee)-EpochDiff) * SecsInDay));
	return Result;
}
// some important Tag Idents (TTagHead.Ident) that we will need to read the most common content of a PTU file
// check the output of this program and consult the tag dictionary if you need more
#define TTTRTagTTTRRecType "TTResultFormat_TTTRRecType"
#define TTTRTagNumRecords  "TTResult_NumberOfRecords"  // Number of TTTR Records in the File;
#define TTTRTagRes         "MeasDesc_Resolution"       // Resolution for the Dtime (T3 Only)
#define TTTRTagGlobRes     "MeasDesc_GlobalResolution" // Global Resolution of TimeTag(T2) /NSync (T3)
#define FileTagEnd         "Header_End"                // Always appended as last tag (BLOCKEND)
// TagTypes  (TTagHead.Typ)
#define tyEmpty8      0xFFFF0008
#define tyBool8       0x00000008
#define tyInt8        0x10000008
#define tyBitSet64    0x11000008
#define tyColor8      0x12000008
#define tyFloat8      0x20000008
#define tyTDateTime   0x21000008
#define tyFloat8Array 0x2001FFFF
#define tyAnsiString  0x4001FFFF
#define tyWideString  0x4002FFFF
#define tyBinaryBlob  0xFFFFFFFF


 int  MKS_inline PicoQuant_header_parser(FILE *fpin) {
	int readResult;
	char* AnsiBuffer;
	WCHAR* WideBuffer;

	// read version
	char Version[8];
	readResult = fread(&Version, 1, sizeof(Version), fpin);
	if (readResult != sizeof(Version))
	{
		PERROR("\nerror reading header, aborted.");
		goto close;
	}

	PINFO("PTU file Header: %s \n", Version);
	// read tagged header
	do
	{
		// This loop is very generic. It reads all header items and displays the identifier and the
		// associated value, quite independent of what they mean in detail.
		// Only some selected items are explicitly retrieved and kept in memory because they are 
		// needed to subsequently interpret the TTTR record data.

		readResult = fread(&TagHead, 1, sizeof(TagHead), fpin);
		if (readResult != sizeof(TagHead))
		{
			PINFO("\nIncomplete File.");
			goto close;
		}
		{
			char readStringBuffer[40];
			strcpy(readStringBuffer, TagHead.Ident);
			if (TagHead.Idx > -1)
			{
				sprintf(readStringBuffer, "%s(%d)", TagHead.Ident, TagHead.Idx);
			}
			printf("\n%-40s", readStringBuffer);
		}

		switch (TagHead.Typ)
		{
		case tyEmpty8:
			printf("<empty Tag>");
			break;
		case tyBool8:
			printf("%s", bool(TagHead.TagValue) ? "True" : "False");
			break;
		case tyInt8:
			printf("%lld", TagHead.TagValue);
			// get some Values we need to analyse records
			if (strcmp(TagHead.Ident, TTTRTagNumRecords) == 0) // Number of records
				NumRecords = TagHead.TagValue;
			if (strcmp(TagHead.Ident, TTTRTagTTTRRecType) == 0) // TTTR RecordType
				RecordType = TagHead.TagValue;
			break;
		case tyBitSet64:
			printf("0x%16.16X", TagHead.TagValue);
			break;
		case tyColor8:
			printf("0x%16.16X", TagHead.TagValue);
			break;
		case tyFloat8:
			printf("%E", *(double*)&(TagHead.TagValue));
			if (strcmp(TagHead.Ident, TTTRTagRes) == 0) {
				// Resolution of delay time for T3
				double TTTRTagRes_spr = *(double*)&(TagHead.TagValue);
				DTRes_pspr = TTTRTagRes_spr* 1e12;
			}
			if (strcmp(TagHead.Ident, TTTRTagGlobRes) == 0) {
				// Global resolution of timetag for T2 mode
				// SYNC rate for T3 mode
				double TTTRTagGlobRes_spr = *(double*)&(TagHead.TagValue);
				TTRes_pspr = TTTRTagGlobRes_spr* 1e12;
			}
			break;

		case tyFloat8Array:
			printf("<Float Array with %d Entries>", TagHead.TagValue / sizeof(double));
			// only seek the Data, if one needs the data, it can be loaded here
			fseek(fpin, (long)TagHead.TagValue, SEEK_CUR);
			break;
		case tyTDateTime:
			time_t CreateTime;
			CreateTime = TDateTime_TimeT(*((double*)&(TagHead.TagValue)));
			printf("%s", asctime(gmtime(&CreateTime)), "\0");
			break;
		case tyAnsiString:
			AnsiBuffer = (char*)calloc((size_t)TagHead.TagValue, 1);
			readResult = fread(AnsiBuffer, 1, (size_t)TagHead.TagValue, fpin);
			if (readResult != TagHead.TagValue)
			{
				printf("\nIncomplete File.");
				free(AnsiBuffer);
				goto close;
			}
			printf("%s", AnsiBuffer);
			free(AnsiBuffer);
			break;
		case tyWideString:
			WideBuffer = (WCHAR*)calloc((size_t)TagHead.TagValue, 1);
			readResult = fread(WideBuffer, 1, (size_t)TagHead.TagValue, fpin);
			if (readResult != TagHead.TagValue)
			{
				printf("\nIncomplete File.");
				free(WideBuffer);
				goto close;
			}
			wprintf(L"%s", WideBuffer);
			free(WideBuffer);
			break;
		case tyBinaryBlob:
			printf("<Binary Blob contains %d Bytes>", TagHead.TagValue);
			// only seek the Data, if one needs the data, it can be loaded here
			fseek(fpin, (long)TagHead.TagValue, SEEK_CUR);
			break;
		default:
			printf("Illegal Type identifier found! Broken file?");
			goto close;
		}
	} while ((strncmp(TagHead.Ident, FileTagEnd, sizeof(FileTagEnd))));
	PINFO("\n-----------------------\n");
	// End Header loading
	bool IsT2;

	// TTTR Record type
	switch (RecordType)
	{
	case rtPicoHarpT2:
		PINFO("PicoHarp T2 data\n");
		IsT2 = true;
		break;
	case rtHydraHarpT2:
		PINFO("HydraHarp V1 T2 data\n");
		IsT2 = true;
		break;
	case rtHydraHarp2T2:
		PINFO("HydraHarp V2 T2 data\n");
		IsT2 = true;
		break;
	case rtTimeHarp260NT2:
		PINFO("TimeHarp260N T2 data\n");
		IsT2 = true;
		break;
	case rtTimeHarp260PT2:
		PINFO("TimeHarp260P T2 data\n");
		IsT2 = true;
		break;
	case rtPicoHarpT3:
		PINFO("PicoHarp T3 data\n");
		IsT2 = false;
		break;
	case rtHydraHarpT3:
		PINFO("HydraHarp V1 T3 data\n");
		IsT2 = false;
		break;
	case rtHydraHarp2T3:
		PINFO("HydraHarp V2 T3 data\n");
		IsT2 = false;
		break;
	case rtTimeHarp260NT3:
		PINFO("TimeHarp260N T3 data\n");
		IsT2 = false;
		break;
	case rtTimeHarp260PT3:
		PINFO("TimeHarp260P T3 data\n");
		IsT2 = false;
		break;
	default:
		PINFO("Unknown time-tag record type: 0x%X\n 0x%X\n ", RecordType);
		goto close;
	}

	
	// set SYNC rate
	if (IsT2) {
		SYNCRate_pspr = 12.49554 * 1000;
	}
	else
		SYNCRate_pspr = TTRes_pspr;

	BytesofRecords = 4;
	// find size

	TTF_header_offset = ftell(fpin);
	
	
	return 0;
close:
	return -1;
ex:
	return -2;
}

/////////////////////////////////////////////
//////////////////////////////////////////////

extern "C" int MKS_inline PARSE_TimeTagFileHeader(char* TTF_filename, int RecordTypetemp)
{
	int ret = -1;
	PINFO("File name: %s", TTF_filename);
	FILE *fpin;
		//open Time-tagged file
		if ((fpin = fopen(TTF_filename, "rb")) == NULL) {
			PERROR("Can not open time-tagged file, aborting.");
			return -1;
		}


		char Magic[8];
		if (fread(&Magic, 1, sizeof(Magic), fpin) != sizeof(Magic)) {
			PERROR("Failed to read header, aborted.");
			return -2;
		}
		
		bool isendlessfile = true;
		if (strncmp(Magic, "PQTTTR", 6) == 0) {
			RecordTypetemp = -1;
			isendlessfile = false;
		}

		//quTAU_FORMAT_BINARY magic is shit
		if (strncmp(Magic,"\x87\xB3\x91\xFA", 4) == 0) {
			RecordTypetemp = 0;
		}
		switch (RecordTypetemp){
		case 0:
			PINFO("Header Parser: quTAU_FORMAT_BINARY \n");
			ret = quTAU_FORMAT_BINARY_header_parser(fpin);
			break;
		
		case 1:
			PINFO("Header Parser: Swebian Instrument \n");
			ret = Swebian_header_parser();
			break;
		case 2:
			//quTAU_FORMAT_COMPRESSED magic is shit
			PINFO("Header Parser: quTAU_FORMAT_COMPRESSED \n");
			ret = quTAU_FORMAT_COMPRESSED_header_parser(fpin);
			break;
		case 3:
			//bh_spc_4bytes magic is shit
			PINFO("Header Parser: bh_spc_4bytes \n");
			ret = bh_4bytes_header_parser(Magic);
			break;
		case -1:
			PINFO("Header Parser: PicoQuant \n");
			ret = PicoQuant_header_parser(fpin);
			break;
		}

		
		fclose(fpin);
		// get file size
		int fh;
		_sopen_s(&fh, TTF_filename, _O_RDONLY, _SH_DENYNO, 0);
		TTF_filesize = _lseeki64(fh, 0L, SEEK_END);
		_close(fh);

		PINFO("Filesize: %lld", TTF_filesize);
		
		if (isendlessfile)
				NumRecords = ((TTF_filesize - TTF_header_offset) / BytesofRecords);
		
		return ret;
}