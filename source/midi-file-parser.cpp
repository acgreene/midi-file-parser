#include <string>
#include <iostream>
#include <fstream>
using namespace std;

// https://www.youtube.com/watch?v=040BKtnDdg0

class midiFile
{
public:
	midiFile()
	{
	}

	midiFile(const string& filename)
	{
		parseFile(filename);
	}

	// takes in a path to MIDI file.
	bool parseFile(const string& filename)
	{
		// open the midi file as a stream.
		ifstream ifs;

		// opens filename and associates it with stream object, sets 
		// filename as input and binary as output.
		ifs.open(filename, fstream::in | ios::binary); 
		if (!ifs.is_open()) return false;
		
		// a lambda function that swaps the byte order of a 32 bit 
		// integer using bitshift operators & bitwise operators. 
		auto swap32 = [](uint32_t n)
		{
			return (((n >> 24) & 0xff) | ((n << 8) & 0xff0000) | ((n >> 8) & 0xff00) | ((n << 24) & 0xff000000));
		};

		// same as above but for 16 bit integers. 
		auto swap16 = [](uint16_t n)
		{
			return ((n >> 8) | (n << 8));
		};

		// reads nLength bytes from the file stream and constructs a 
		// text string.
		auto ReadString = [&ifs](uint32_t nLength)
		{
			string s;
			for (uint32_t i = 0; i < nLength; i++) s += ifs.get();
			return s;
		};

		auto readValue = [&ifs]()
		{
			uint32_t nValue = 0;
			uint8_t nByte = 0;
			
			// read byte.
			nValue = ifs.get();

			// check MSP, if set, more bytes need reading.
			if (nValue & 0x80)
			{
				//extract bottom 7 bits of read byte.
				nValue &= 0x7f;

				do
				{
					// read next byte.
					nByte = ifs.get();

					//construct value by setting bottom 7 bits, then 
					//shifting 7 bits. 
					nValue = (nValue << 7) | (nByte & 0x7f);

				} while (nByte & 0x80); //loop while read byte MSB is 1. 
			}

			//return final construction (always 32-bit unsigned integer internally)
			return nValue;
		};
	}
};