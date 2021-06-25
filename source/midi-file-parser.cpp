#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <list>
#include <algorithm>

using namespace std;

// https://www.youtube.com/watch?v=040BKtnDdg0


struct midiEvent
{
	enum class type
	{
		noteOff,
		noteOn, 
		other
	} event;

	uint8_t key = 0;
	uint8_t velocity = 0;
	uint32_t deltaTick = 0;
};

struct midiNote
{
	uint8_t key = 0;
	uint8_t velocity = 0;
	uint32_t start = 0;
	uint32_t duration = 0;
};

struct midiTrack
{
	string name;
	string instrument;
	vector<midiEvent> events;
	vector<midiNote> notes;
	uint8_t nMaxNote = 64;
	uint8_t nMinNote = 64;
};

class midiFile
{
public:
	enum eventName : uint8_t
	{
		voiceNoteOff = 0x80,
		voiceNoteOn = 0x90,
		voiceAfterTouch = 0xA0,
		voiceControlChange = 0xB0,
		voiceProgramChange = 0xC0,
		voiceChannelPressure = 0xD0,
		voicePitchBend = 0xE0,
		systemExclusive = 0xF0,
	};

	enum MetaEventName : uint8_t
	{
		MetaSequence = 0x00,
		MetaText = 0x01,
		MetaCopyright = 0x02,
		MetaTrackName = 0x03,
		MetaInstrumentName = 0x04,
		MetaLyrics = 0x05,
		MetaMarker = 0x06,
		MetaCuePoint = 0x07,
		MetaChannelPrefix = 0x20,
		MetaEndOfTrack = 0x2F,
		MetaSetTempo = 0x51,
		MetaSMPTEOffset = 0x54,
		MetaTimeSignature = 0x58,
		MetaKeySignature = 0x59,
		MetaSequencerSpecific = 0x7F,
	};

public:

	uint32_t m_nTempo = 0;
	uint32_t m_nBPM = 0;
	vector<midiTrack> tracks;

	midiFile()
	{
	}

	midiFile(const string& filename)
	{
		parseFile(filename);
	}

	void Clear(){}

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

			// check most significant bit of the byte, if set, more 
			// bytes need reading.
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
			
			//return final construction (always 32-bit unsigned int)
			return nValue;
		};

		//parse midi file
		uint32_t n32 = 0;
		uint16_t n16 = 0;

		//read midi header (fixed size)
		ifs.read((char*)&n32, sizeof(uint32_t));
		uint32_t nFileID = swap32(n32);

		ifs.read((char*)&n32, sizeof(uint32_t));
		uint32_t nHeaderLength = swap32(n32);

		ifs.read((char*)&n16, sizeof(uint16_t));
		uint16_t nFormat = swap16(n16);

		// number of midi tracks the file contains. the tracks contain 
		// the midi events too.
		ifs.read((char*)&n16, sizeof(uint16_t));
		uint16_t nTrackChunks = swap16(n16);

		ifs.read((char*)&n16, sizeof(uint16_t));
		uint16_t nDivision = swap16(n16);

		for (uint16_t nChunk = 0; nChunk < nTrackChunks; nChunk++) 
		{
			cout << "===== NEW TRACK" << endl;

			// read track header
			ifs.read((char*)&n32, sizeof(uint32_t));
			uint32_t nTrackID = swap32(n32);

			ifs.read((char*)&n32, sizeof(uint32_t));
			uint32_t nTrackLength = swap32(n32);

			bool endOfTrack = false;
			
			tracks.push_back(midiTrack());

			uint32_t nWallTime = 0;
			uint8_t nPreviousStatus = 0;

			while (!ifs.eof() && !endOfTrack)
			{
				// all midi events contain a timecode and a status byte
				uint32_t nStatusTimeDelta = 0;
				uint8_t nStatus = 0;

				// read timecode
				nStatusTimeDelta = readValue();
				nStatus = ifs.get();

				if (nStatus < 0x80)
				{
					nStatus = nPreviousStatus;
					// have the midi stream go back 1 byte since we just lost
					// it by setting it to the previous byte. 
					ifs.seekg(-1, ios_base::cur);
				}

				if ((nStatus & 0xF0) == eventName::voiceNoteOff)
				{
					nPreviousStatus = nStatus;
					uint8_t nChannel = nStatus & 0x0F;
					uint8_t nNoteID = ifs.get();
					uint8_t nNoteVelocity = ifs.get();
					tracks[nChunk].events.push_back({midiEvent::type::noteOff, 
													 nNoteID, nNoteVelocity, nStatusTimeDelta});
				}
				else if ((nStatus & 0xF0) == eventName::voiceNoteOn)
				{
					nPreviousStatus = nStatus;
					uint8_t nChannel = nStatus & 0x0F;
					uint8_t nNoteID = ifs.get();
					uint8_t nNoteVelocity = ifs.get();

					if (nNoteVelocity == 0)
						tracks[nChunk].events.push_back({midiEvent::type::noteOff, 
													 nNoteID, nNoteVelocity, nStatusTimeDelta});
					else
						tracks[nChunk].events.push_back({midiEvent::type::noteOn, 
													 nNoteID, nNoteVelocity, nStatusTimeDelta});								 
				}
				else if ((nStatus & 0xF0) == eventName::voiceAfterTouch)
				{
					nPreviousStatus = nStatus;
					uint8_t nChannel = nStatus & 0x0F;
					uint8_t nNoteID = ifs.get();
					uint8_t nNoteVelocity = ifs.get();
					tracks[nChunk].events.push_back({midiEvent::type::other});
				}
				else if ((nStatus & 0xF0) == eventName::voiceControlChange)
				{
					nPreviousStatus = nStatus;
					uint8_t nChannel = nStatus & 0x0F;
					uint8_t nNoteID = ifs.get();
					uint8_t nNoteVelocity = ifs.get();
					tracks[nChunk].events.push_back({midiEvent::type::other});
				}
				else if ((nStatus & 0xF0) == eventName::voiceProgramChange)
				{
					nPreviousStatus = nStatus;
					uint8_t nChannel = nStatus & 0x0F;
					uint8_t nProgramID = ifs.get();
					tracks[nChunk].events.push_back({midiEvent::type::other});
				}
				else if ((nStatus & 0xF0) == eventName::voiceChannelPressure)
				{
					nPreviousStatus = nStatus;
					uint8_t nChannel = nStatus & 0x0F;
					uint8_t nChannelPressure = ifs.get();
					tracks[nChunk].events.push_back({midiEvent::type::other});
				}
				else if ((nStatus & 0xF0) == eventName::voicePitchBend)
				{
					nPreviousStatus = nStatus;
					uint8_t nChannel = nStatus & 0x0F;
					uint8_t nLS7B = ifs.get();
					uint8_t nMS7B = ifs.get();
					tracks[nChunk].events.push_back({midiEvent::type::other});
				}
				else if ((nStatus & 0xF0) == eventName::systemExclusive)
				{
					nPreviousStatus = 0;

					if (nStatus == 0xFF)
					{
						// Meta Message
						uint8_t nType = ifs.get();
						uint8_t nLength = readValue();

						switch (nType)
						{
						case MetaSequence:
							cout << "Sequence Number: " << ifs.get() << ifs.get() << endl;
							break;
						case MetaText:
							cout << "Text: " << ReadString(nLength) << endl;
							break;
						case MetaCopyright:
							cout << "Copyright: " << ReadString(nLength) << endl;
							break;
						case MetaTrackName:
							tracks[nChunk].name = ReadString(nLength);
							cout << "Track Name: " << tracks[nChunk].name << endl;							
							break;
						case MetaInstrumentName:
							tracks[nChunk].instrument = ReadString(nLength);
							cout << "Instrument Name: " << tracks[nChunk].instrument << endl;
							break;
						case MetaLyrics:
							cout << "Lyrics: " << ReadString(nLength) << endl;
							break;
						case MetaMarker:
							cout << "Marker: " << ReadString(nLength) << endl;
							break;
						case MetaCuePoint:
							cout << "Cue: " << ReadString(nLength) << endl;
							break;
						case MetaChannelPrefix:
							cout << "Prefix: " << ifs.get() << endl;
							break;
						case MetaEndOfTrack:
							endOfTrack = true;
							break;
						case MetaSetTempo:
							// Tempo is in microseconds per quarter note	
							if (m_nTempo == 0)
							{
								(m_nTempo |= (ifs.get() << 16));
								(m_nTempo |= (ifs.get() << 8));
								(m_nTempo |= (ifs.get() << 0));
								m_nBPM = (60000000 / m_nTempo);
								std::cout << "Tempo: " << m_nTempo << " (" << m_nBPM << "bpm)" << std::endl;
							}
							break;
						case MetaSMPTEOffset:
							std::cout << "SMPTE: H:" << ifs.get() << " M:" << ifs.get() << " S:" << ifs.get() << " FR:" << ifs.get() << " FF:" << ifs.get() << std::endl;
							break;
						case MetaTimeSignature:
							std::cout << "Time Signature: " << ifs.get() << "/" << (2 << ifs.get()) << std::endl;
							std::cout << "ClocksPerTick: " << ifs.get() << std::endl;

							// A MIDI "Beat" is 24 ticks, so specify how many 32nd notes constitute a beat
							std::cout << "32per24Clocks: " << ifs.get() << std::endl;
							break;
						case MetaKeySignature:
							std::cout << "Key Signature: " << ifs.get() << std::endl;
							std::cout << "Minor Key: " << ifs.get() << std::endl;
							break;
						case MetaSequencerSpecific:
							std::cout << "Sequencer Specific: " << ReadString(nLength) << std::endl;
							break;
						default:
							std::cout << "Unrecognised MetaEvent: " << nType << std::endl;
						}
				}

				if (nStatus == 0xF0)
				{
					// System Exclusive Message Begin
					cout << "System Exclusive Begin: " << ReadString(readValue())  << endl;
				}

				if (nStatus == 0xF7)
				{
					// System Exclusive Message Begin
					cout << "System Exclusive End: " << ReadString(readValue()) << endl;
				}
				else
				{
					cout << "Unrecognized status byte: " << nStatus << endl;
				}
			}
		}

		// Convert Time Events to Notes
		for (auto& track : tracks)
		{
			uint32_t nWallTime = 0;

			list<midiNote> listNotesBeingProcessed;

			for (auto& event : track.events)
			{
				nWallTime += event.deltaTick;

				if (event.event == midiEvent::type::noteOn)
				{
					// New Note
					listNotesBeingProcessed.push_back({ event.key, event.velocity, nWallTime, 0 });
				}

				if (event.event == midiEvent::type::noteOff)
				{
					auto note = find_if(listNotesBeingProcessed.begin(), listNotesBeingProcessed.end(), [&](const midiNote& n) { return n.key == event.key; });
					if (note != listNotesBeingProcessed.end())
					{
						note->duration = nWallTime - note->start;
						track.notes.push_back(*note);
						track.nMinNote = min(track.nMinNote, note->key);
						track.nMaxNote = max(track.nMaxNote, note->key);
						listNotesBeingProcessed.erase(note);
					}
				}
			}
		}

		return true;

	}
}
};