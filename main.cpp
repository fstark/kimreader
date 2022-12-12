#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <math.h>

std::string from_time( double t )
{
    int s = t;
    t -= s;
    int m = s/60;
    s -= m*60;

    return std::to_string( m )+":"+std::to_string(s)+":"+std::to_string( (int)(t*1000) );
}

bool verbose = false;

typedef uint8_t sample_t;

typedef enum
{
    k3700, k2400
}   e_freq;


constexpr double DELTA=1/22050.0/2;

struct Parser
{
    static const sample_t MID = 128;
    static constexpr double width_9 = (7.452/3)/9/1000;
    static constexpr double width_6 = (7.452/3)/6/1000;
    static constexpr double width_epsilon = width_9/3;

    double time = 0;

    bool state = true;    //  We start "lower than MID"

    double last_time = 0;

    std::vector<bool> result;

    void add_bit( int bit )
    {
        result.push_back( bit );
        std::cout << bit;
    }

    double is_6_ = true;     //  We start at the same 6 to 9 pulse sequence

    int counter[2] = { 0, 0 };

    void add_pulse( bool is_6 )
    {
        counter[is_6]++;
        // std::cout << (is_6_?"6":"9");
        if (is_6_ && !is_6)
        {
            int c9 = counter[false];
            int c6 = counter[true];
            if (c9==9 || c9==11) c9 = 10;
            if (c9==17 || c9==19) c9 = 18;
            if (c6==10 || c6==12) c6 = 11;
            if (c6==5 || c6==7) c6 = 6;

            if (c9==10 && c6==11) add_bit( 1 );
            else if (c9==18 && c6==6) add_bit( 0 );
            else std::cout << "? (" << from_time(time) << " " << counter[false] << "/" << counter[true] << ")";

            counter[0] = counter[1] = 0;
        }

        is_6_ = is_6;
    }

    //  Called for each zero-crossing
    void zero_cross()
    {
        double w = time-last_time;
        last_time = time;

// std::cout << "(" << w << " " << width_9 << ")";

        if (::fabs(w-width_9)<width_epsilon)
            add_pulse( false );
        else if (::fabs(w-width_6)<width_epsilon)
            add_pulse( true );
        else if (verbose)
            std::cout << "\nZERO CROSSING AT " << from_time(time) << " : width = " << w <<
            " 9 = [" << width_9-width_epsilon << "-" << width_9+width_epsilon << "] "
            " 6 = [" << width_6-width_epsilon << "-" << width_6+width_epsilon << "]\n";
            else std::cout << "*";
    }

    //  Called to add each sample
    void add( bool low )
    {
        if (state != low)           //  We changed state from LOW->HIGH or HIGH->LOW
        {
            state = low;
            if (!low)               //  Now in HIGH
                zero_cross();       //  Zero cross
        }
    }

    //  Called to add each sample
    void add( const sample_t sample )
    {
        time += DELTA;
        add( sample<MID );
    }
};

std::string string_from_bits( const std::vector<bool> b, int delta = 0 )
{
    uint8_t ch;
    int bit_ix=delta;
    std::string result;

    for (auto bit:b)
    {
        ch/=2;
        if (bit) ch += 128;
        bit_ix++;
        if (bit_ix==8)
        {
            bit_ix = 0;
            result.push_back( ch );
        }
    }

    return result;
}

void parse( const sample_t *data, size_t sample_count )
{
    Parser p;
    for (size_t i=0;i!=sample_count;i++)
        p.add( data[i] );

    for (int i=0;i!=8;i++)
    {
        std::cout << "\n\n\n------------------------------------- " << i << "\n\n\n";
        std::cout << string_from_bits( p.result, i );
    }

    return;
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
