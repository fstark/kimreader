#include <iostream>
#include <fstream>
#include <cstdint>

typedef uint8_t sample_t;

typedef enum
{
    k3700, k2400
}   e_freq;

void parse( const sample_t *data, size_t sample_count )
{
    bool state = false;
    e_freq current = k3700;
    const sample_t CUTOFF = 128;

    size_t last = 0;

    size_t count[2] = { 0, 0 };

    uint8_t result = 0;

    size_t bit_read = 6;

    for (size_t i=0;i!=sample_count;i++)
    {
        sample_t s = data[i];
        if (!state)
        {
            if (s>=CUTOFF)
            {
                state = true;
                size_t delta = i-last;
                last = i;
                // std::cout << "(" << i << " " << delta << ")";

                if (delta>10 && delta<20)
                {   if (delta<=14)
                    {
                        if (current==k2400)
                        {
                            //  bit boundary
                            if (count[0]>count[1])
                            {   result *= 2;
                                // std::cout << "0";
                            }
                            else
                            {   result *= 2;
                                result ++;
                                // std::cout << "1";
                            }
                            if (bit_read++==7)
                            {
                                // std::cout << " -> " << i << " " << i/22050.0 << ":" << (int)result << "\n";
                                std::cout << (char)(result&0x7f);
                                result = 0;
                                bit_read = 0;
                            }

                            count[0] = count[1] = 0;
                        }
                        current = k3700;
                        count[0]++;
                    }
                    else
                    {
                        current = k2400;
                        count[1]++;
                    }
                }
            }
        }
        else
        {
            if (s<CUTOFF)
            {
                state = false;
            }
        }
    }
}

using namespace std;

const int BUFF_SIZE = 1024;

int main(int argc, char* argv[])
{
  // Check if a file was provided as an argument
  if (argc < 2)
  {
    cerr << "Please provide a WAV file to read" << endl;
    return 1;
  }

  // Open the WAV file in binary mode
  ifstream file(argv[1], ios::binary);
  if (!file.is_open())
  {
    cerr << "Could not open file " << argv[1] << endl;
    return 1;
  }

  // Read the WAV file header
  char buffer[BUFF_SIZE];

  // Read the chunk ID, should be "RIFF"
  file.read(buffer, 4);
  if (string(buffer, 4) != "RIFF")
  {
    cerr << "Invalid WAV file" << endl;
    return 1;
  }

  // Read the file size
  uint32_t file_size;
  file.read((char*)&file_size, sizeof(file_size));

  // Read the file format, should be "WAVE"
  file.read(buffer, 4);
  if (string(buffer, 4) != "WAVE")
  {
    cerr << "Invalid WAV file" << endl;
    return 1;
  }

  // Read the format chunk ID, should be "fmt "
  file.read(buffer, 4);
  if (string(buffer, 4) != "fmt ")
  {
    cerr << "Invalid WAV file" << endl;
    return 1;
  }

  // Read the format chunk size
  uint32_t format_chunk_size;
  file.read((char*)&format_chunk_size, sizeof(format_chunk_size));

  // Read the audio format
  uint16_t audio_format;
  file.read((char*)&audio_format, sizeof(audio_format));

  // Read the number of channels
  uint16_t num_channels;
  file.read((char*)&num_channels, sizeof(num_channels));

  // Read the sample rate
  uint32_t sample_rate;
  file.read((char*)&sample_rate, sizeof(sample_rate));

  // Read the byte rate
  uint32_t byte_rate;
  file.read((char*)&byte_rate, sizeof(byte_rate));

  // Read the block align
  uint16_t block_align;
  file.read((char*)&block_align, sizeof(block_align));

  // Read the bits per sample
  uint16_t bits_per_sample;
  file.read((char*)&bits_per_sample, sizeof(bits_per_sample));

  // Print the file size, audio format, and number of channels
  cout << "File size: " << file_size << " bytes" << endl;
  cout << "Audio format: " << audio_format << " bytes" << endl;
  cout << "#channels: " << num_channels << " bytes" << endl;
  cout << "Sample rate: " << sample_rate << " bytes" << endl;
  cout << "Byte size: " << byte_rate << " bytes" << endl;
  cout << "Bits per sample: " << bits_per_sample << " bytes" << endl;

  file.read(buffer, 4);
  if (string(buffer, 4) != "data")
  {
    cerr << "Invalid WAV file" << endl;
    return 1;
  }

    int32_t sample_count;
    file.read((char*)&sample_count, 4);

    sample_t *data = new sample_t[sample_count];
    file.read( (char *)data, sample_count );

    parse( data, sample_count );

    delete[] data;

    return EXIT_SUCCESS;
}
