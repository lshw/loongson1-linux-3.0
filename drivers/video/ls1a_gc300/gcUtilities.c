#include "gcSdk.h"

int _MIN(const int n1, const int n2)
{
	return (n1 < n2) ? n1 : n2;
}

int _MAX(const int n1, const int n2)
{
	return (n1 > n2) ? n1 : n2;
}

// Extract bits from an 8-bit value.
UINT8 GETBITS8(const UINT8 Data, const int Start, const int End)
{
	const int _Start = _MIN(Start, End);
	const int _End = _MAX(Start, End);
	const int _Size = _End - _Start + 1;
	const UINT8 _Mask = ((UINT8) ~0) >> ((8 - _Size) & 7);
	return (Data >> _Start) & _Mask;
}

// Extract bits from a 16-bit value.
UINT16 GETBITS16(const UINT16 Data, const int Start, const int End)
{
	const int _Start = _MIN(Start, End);
	const int _End = _MAX(Start, End);
	const int _Size = _End - _Start + 1;
	const UINT16 _Mask = ((UINT16) ~0) >> ((16 - _Size) & 15);
	return (Data >> _Start) & _Mask;
}

// Extract bits from a 32-bit value.
UINT32 GETBITS32(const UINT32 Data, const int Start, const int End)
{
	const int _Start = _MIN(Start, End);
	const int _End = _MAX(Start, End);
	const int _Size = _End - _Start + 1;
	const UINT32 _Mask = ((UINT32) ~0) >> ((32 - _Size) & 31);
	return (Data >> _Start) & _Mask;
}

// Extract bits from a 64-bit value.
UINT64 GETBITS64(const UINT64 Data, const int Start, const int End)
{
	const int _Start = _MIN(Start, End);
	const int _End = _MAX(Start, End);
	const int _Size = _End - _Start + 1;
	const UINT64 _Mask = ((UINT64) ~0) >> ((64 - _Size) & 63);
	return (Data >> _Start) & _Mask;
}

// Set bits in an 8-bit value.
UINT8 SETBITS8(UINT8* Data, const int Start, const int End, const UINT8 Value)
{
	const int _Start = _MIN(Start, End);
	const int _End = _MAX(Start, End);
	const int _Size = _End - _Start + 1;
	const UINT8 _Mask = ((UINT8) ~0) >> ((8 - _Size) & 7);
	*Data &= ~(_Mask << _Start);
	*Data |= (Value & _Mask) << _Start;
	return *Data;
}

// Set bits in a 16-bit value.
UINT16 SETBITS16(UINT16* Data, const int Start, const int End, const UINT16 Value)
{
	const int _Start = _MIN(Start, End);
	const int _End = _MAX(Start, End);
	const int _Size = _End - _Start + 1;
	const UINT16 _Mask = ((UINT16) ~0) >> ((16 - _Size) & 15);
	*Data &= ~(_Mask << _Start);
	*Data |= (Value & _Mask) << _Start;
	return *Data;
}

// Set bits in a 32-bit value.
UINT32 SETBITS32(UINT32* Data, const int Start, const int End, const UINT32 Value)
{
	const int _Start = _MIN(Start, End);
	const int _End = _MAX(Start, End);
	const int _Size = _End - _Start + 1;
	const UINT32 _Mask = ((UINT32) ~0) >> ((32 - _Size) & 31);
	*Data &= ~(_Mask << _Start);
	*Data |= (Value & _Mask) << _Start;
	return *Data;
}

// Set bits in a 64-bit value.
UINT64 SETBITS64(UINT64* Data, const int Start, const int End, const UINT64 Value)
{
	const int _Start = _MIN(Start, End);
	const int _End = _MAX(Start, End);
	const int _Size = _End - _Start + 1;
	const UINT64 _Mask = ((UINT64) ~0) >> ((64 - _Size) & 63);
	*Data &= ~(_Mask << _Start);
	*Data |= (Value & _Mask) << _Start;
	return *Data;
}

void gcPoke(UINT32 Address, UINT32 Data)
{
/*	if(!(Address & 0x80000000))
		Address |= 0xA0000000;
	if((Data & 0xFF000000)==0xA2000000) {
		Data &= 0x0FFFFFFF;
	}
*/
	*(PUINT32) Address = Data;
}

UINT32 gcPeek(UINT32 Address)
{
/*	if(!(Address & 0x80000000))
		Address |= 0xA0000000;
*/
	return *(PUINT32) Address;
}

UINT32 gcSurfaceWidth(gcSURFACEINFO* Surface)
{
	return Surface->rect.right - Surface->rect.left;
}

UINT32 gcSurfaceHeight(gcSURFACEINFO* Surface)
{
	return Surface->rect.bottom - Surface->rect.top;
}

