#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <math.h>
#include <algorithm>
#include <memory>
#include <cstring>

std::string from_time( double t )
{
    int s = t;
    t -= s;
    int m = s/60;
    s -= m*60;

    return std::to_string( m )+":"+std::to_string(s)+":"+std::to_string( (int)(t*1000) );
}

bool silent = true;
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

        //  The time at which we fond the last valid transition
    double last_valid_bit = -1;
    bool first = true;

    void add_bit( int bit )
    {
        if (!first)
            while (time-last_valid_bit>10.0/1000)
            {
                std::cout << "#";
                    //  We insert an arbitrary bit
                result.push_back( 0 );
                last_valid_bit += 7.452/1000;
            }
        first = false;
        last_valid_bit = time;

        result.push_back( bit );
        if (!silent)
        {
            std::cout << bit;
            // std::cout << "(" << from_time(time) << ") ";
        }
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
            else
            {
                //  We were unable to find if this is a 0 or a 1
                if (verbose)
                    std::cout << "? (" << from_time(time) << " " << counter[false] << "/" << counter[true] << ")";
                else
                    if (!silent) std::cout << "?";

                // //  We add a '0' anyway
                // add_bit( 0 );
                // add_bit( 1 );
                // add_bit( 0 );
            }

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
        else
        {
            //  We have a zero crossing that is not of the correct frequency
            if (verbose)
                std::cout << "\nZERO CROSSING AT " << from_time(time) << " : width = " << w <<
                " 9 = [" << width_9-width_epsilon << "-" << width_9+width_epsilon << "] "
                " 6 = [" << width_6-width_epsilon << "-" << width_6+width_epsilon << "]\n";
            else
                if (!silent) std::cout << "*";
        }
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

std::string string_from_bits( std::vector<bool>::const_iterator b, const std::vector<bool>::const_iterator e )
{
    uint8_t ch;
    int bit_ix=0;
    std::string result;

    while (b!=e)
    {
        ch/=2;
        if (*b++) ch += 128;
        bit_ix++;
        if (bit_ix==8)
        {
            bit_ix = 0;
            result.push_back( ch );
        }
    }

    return result;
}

// struct kim_data
// {
//     uint8_t id;
//     uint16_t adrs;
//     std::vector<uint8_t> data;
//     uint16_t checksum;
// };

template <class I>
bool compare_bits( I b, I e, const std::vector<bool> &bits )
{
    if (e-b<bits.size())
        return false;
    for (auto bit:bits)
        if (*b++!=bit)
            return false;
    return true;
}

template <class I>
I find_bits( I b, I e, const std::vector<bool> &bits )
{
    while (e-b>=bits.size())
    {   if (compare_bits(b,b+bits.size(),bits))
            return b;
        b++;
    }
    return e;   
}

std::unique_ptr<char * /*kim_data*/> kim_data_from_bits( const std::vector<bool> &encoded )
{

    //  Find first 'on' bit
    auto b = std::begin(encoded);
    auto e = std::end(encoded);

loop:
    //  Lookup for SYN ('00010110')
    b = find_bits( b,e, { false, true, true, false, true, false, false, false } );

    if (b==e)
        return nullptr; //  No on bits left
    
    //  Scan by 8 until zero found
    while (compare_bits( b, e, { false, true, true, false, true, false, false, false } ))
            b += 8;

    //  check for '*' ('00101010')
    if (!compare_bits( b, e, { false, true, false, true, false, true, false, false } ))
        goto loop;  //  evil

    b += 8;

    std::cout << string_from_bits( b, e ) << "\n";

    return nullptr;
}

void parse( const std::vector<sample_t> data )
{
    Parser p;
    for (auto s:data)
        p.add( s );

    // for (int i=0;i!=8;i++)
    // {
    //     std::cout << "\n\n\n------------------------------------- " << i << "\n\n\n";
    //     std::cout << string_from_bits( p.result, i );
    // }

    auto start = kim_data_from_bits( p.result );

    return;
}



std::vector<sample_t> normalize( const std::vector<sample_t> &data, int width=0 )
{
    if (width==0)
        return data;

    if (data.size()<2*width+1)
        return {};

    auto b = std::begin( data );
    auto e = std::end( data );
    std::vector<sample_t> result;

    for (auto d=b+width;d!=e-width;d++)
    {
        sample_t min = 255;
        sample_t max = 0;
        int sum = 0;
        for (auto p=d-width;p!=d+width;p++)
        {
            min = std::min(min,*p);
            max = std::max(max,*p);
            sum += *p;
        }
        double value = *d;
        double avg = sum/(2*width+1);

        if (value>avg)
            value = 255;
            // value = (value-avg)/(max-avg+1)*127+128;
        else
            // value = 127-(avg-value)/(avg-min+1)*127;
            value = 0;

       result.push_back( value );

        // for (int i=0;i<value;i++)
        //     std::cout << "*";
        // std::cout << "\n";
    }

    // while (b!=e)
    //     result.push_back( *b++ );

    return result;
}







bool bool_from_string( const std::string s )
{
    if (s=="true")
        return true;
    return false;
}

using namespace std;

const int BUFF_SIZE = 1024;

int main(int argc, char* argv[])
{
    int smooth = 0;
    const char *file_name = "input.wav";

    argc--;
    argv++;

    while (argc)
    {
        if (!strcmp(*argv,"--help"))
        {
            std::cerr << "kimreader [--silent true|false] [--verbose true|false] [--smooth <NUM>] file.wav\n";
            return EXIT_FAILURE;
        }
        else if (!strcmp(*argv,"--smooth"))
        {
            argc--;
            argv++;
            smooth = ::atoi( *argv );
        }
        else if (!strcmp(*argv,"--silent"))
        {
            argc--;
            argv++;
            silent = ::bool_from_string( *argv );
        }
       else if (!strcmp(*argv,"--verbose"))
        {
            argc--;
            argv++;
            verbose = ::bool_from_string( *argv );
        }
        else
            file_name = *argv;
        argc--;
        argv++;
    }

  // Open the WAV file in binary mode
  ifstream file(file_name, ios::binary);
  if (!file.is_open())
  {
    cerr << "Could not open file " << file_name << endl;
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
//   cout << "File size: " << file_size << " bytes" << endl;
//   cout << "Audio format: " << audio_format << " bytes" << endl;
//   cout << "#channels: " << num_channels << " bytes" << endl;
//   cout << "Sample rate: " << sample_rate << " bytes" << endl;
//   cout << "Byte size: " << byte_rate << " bytes" << endl;
//   cout << "Bits per sample: " << bits_per_sample << " bytes" << endl;

  file.read(buffer, 4);
  if (string(buffer, 4) != "data")
  {
    cerr << "Invalid WAV file" << endl;
    return 1;
  }

    int32_t sample_count;
    file.read((char*)&sample_count, 4);

    sample_t *raw_data = new sample_t[sample_count];
    file.read( (char *)raw_data, sample_count );
    std::vector<sample_t> data{ raw_data, raw_data+sample_count };
    delete[] raw_data;

    auto norm = normalize( data, smooth );

    parse( norm );


    return EXIT_SUCCESS;
}
