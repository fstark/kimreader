#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <math.h>
#include <algorithm>
#include <memory>
#include <cstring>
#include <cassert>

std::string from_time( double t )
{
    int s = t;
    t -= s;
    int m = s/60;
    s -= m*60;

    char buffer[1024];

    ::sprintf( buffer, "%02d:%02d:%02d.%02d", 0, m, s, (int)(t*1000) );

    return buffer;
}

bool silent = true;
bool verbose = false;

typedef uint8_t sample_t;

/// @brief Compare bits starting at b to find if they match the 'bits' pattern
/// @return true if bits found
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

/// @brief starting at b, scans to find the bits with the specified stride
/// @return 
template <class I>
I find_bits( I b, I e, const std::vector<bool> &bits, size_t stride = 1 )
{
    while (e-b>=bits.size())
    {   if (compare_bits(b,b+bits.size(),bits))
            return b;
        b += stride;
    }
    return e;   
}

struct fix_t
{
    size_t bit_location;    //  location of the corrupted bit in the bitstream
    double source_ts;       //  Timestamp in the source
};


/// @brief  This is a bitstream, with potentially some unknown bits (but we know where they are)
class bitstream
{
    std::vector<bool> bits_;

    std::vector<fix_t> errors_;

public:
    bitstream( const std::vector<bool> &bits, const std::vector<fix_t> &errors )
        : bits_{ bits }, errors_{ errors }
    {
    }

    /// @brief How many different ways to "fix" the bitstream
    /// @return The number of different values that the 'bits' member function can take
    size_t fix_count() const
    {
        return 1<<errors_.size();
    }

    /// @brief Returns the bits with a certain fix
    /// @param fix a number between 0 and fix_count()-1 that defines how to fill the missing bits 
    /// @return a vector of bits with no unknown bits
    std::vector<bool> bits( size_t fix ) const
    {
        assert( fix<fix_count() );

        std::vector<bool> result;
        size_t ix = 0;

        result = bits_;

        for (auto e:errors_)
            result[e.bit_location] = fix&(1<<ix++);

        return result;
    }

    bitstream slice( size_t start, size_t len ) const
    {
        assert( start+len<=bits_.size() );

        std::vector<fix_t> errors;
        for (auto e:errors_)
            if (e.bit_location>=start && e.bit_location<start+len)
                errors.push_back( { e.bit_location-start, e.source_ts } );

        return bitstream( std::vector<bool>{ std::begin(bits_)+start, std::begin(bits_)+start+len }, errors );
    }

    /// @brief Find the position of the char in the steam of bits. Use little endian bits coding.
    /// @return the bit position where we found the bit pattern
    size_t index_of( uint8_t c, bool &found, size_t after=0, size_t stride=1 ) const
    {
        std::vector<bool> bits;
        for (size_t i=0;i!=8;i++)
            bits.push_back( c&(1<<i) );

        // for (auto b:bits)
        // {
        //     std::cout << (int)b;;
        // }
        // std::cout << "\n";

        auto res = find_bits( std::begin(bits_), std::end(bits_), bits, stride );
        if (res==std::end(bits_))
        {
            found = false;
            return 0;
        }

        found = true;
        return res-std::begin(bits_);
    }

    void patch( std::string patch_instuctions )
    {
        if (errors_.size()>0)
        {
            std::vector<fix_t> new_errors;

            if (patch_instuctions=="")
                patch_instuctions = "x";

            std::cout << "Location in bitstream for corrupted segments:\n";
            for (int i=0;i!=errors_.size();i++)
            {
                auto e = errors_[i];
                std::cout << "  " << from_time( e.source_ts ) << "-" << from_time( e.source_ts+7.452/1000 ) << " -- bit #" << e.bit_location;
                switch (patch_instuctions[i%patch_instuctions.size()])
                {
                    case '0':
                        bits_[e.bit_location] = 0;
                        std::cout << " inserted 0\n";
                        break;
                    case '1':
                        bits_[e.bit_location] = 1;
                        std::cout << " inserted 1\n";
                        break;
                    case 'x':
                        bits_[e.bit_location] = 1;
                        std::cout << " unchanged\n";
                        new_errors.push_back( e );
                        break;
                }
            }

            errors_ = new_errors;
        }
    }

};

//  Tests for bitstream object
void test_bitstream()
{
    bitstream b0{ { 0, 0, 0, 0 }, { { 2, 0 } } };
    assert( b0.fix_count()==2 );
    assert( (b0.bits(0)==std::vector<bool>{ 0, 0, 0, 0 }) );
    assert( (b0.bits(1)==std::vector<bool>{ 0, 0, 1, 0 }) );

    bitstream b1{ { 1, 0, 1, 0 }, { { 1, 0 } , { 3, 0.1 }  } };
    assert( b1.fix_count()==4 );
    assert( (b1.bits(0)==std::vector<bool>{ 1, 0, 1, 0 }) );
    assert( (b1.bits(1)==std::vector<bool>{ 1, 1, 1, 0 }) );
    assert( (b1.bits(2)==std::vector<bool>{ 1, 0, 1, 1 }) );
    assert( (b1.bits(3)==std::vector<bool>{ 1, 1, 1, 1 }) );

    bitstream b2{ { 0, 0, 0, 0, 0, 0 }, { { 1, 0} , { 3, 0.1} , { 5, 0.2} } };
    auto b3 = b2.slice( 1, 3 );
    assert( b3.fix_count()==4 );
    assert( (b3.bits(3)==std::vector<bool>{ 1, 0, 1 }) );

    bitstream b4{ { 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 }, {} };
    bool found;
    auto r4 = b4.index_of( 0x04, found );
    assert( found );
    assert( r4==4 );
}

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

    std::vector<fix_t> fixes;

    void add_bit( int bit )
    {
        if (!first)
            while (time-last_valid_bit>10.0/1000)
            {
                if (!silent)
                    std::cout << "#";
                    //  We insert an arbitrary bit
                fixes.push_back( { result.size(), last_valid_bit } );
                result.push_back( 1 );
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

    //  Converts into a bitstream (we should do everything on a bitstream in reality)
    bitstream get_bitstream() 
    {
        return bitstream{ result, fixes };
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

std::vector<uint8_t> ascii_hex_from_bits( std::vector<bool>::const_iterator b, const std::vector<bool>::const_iterator e )
{
    uint8_t ch;
    int bit_ix=0;
    std::vector<uint8_t> result;

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

bool byte_from_hex1( char c, uint8_t &result )
{
    if (c>='0' && c<='9')
    {
        result = c-'0';
        return true;
    }

    if (c>='A' && c<='F')
    {
        result = c-'A'+10;
        return true;
    }

    if (!silent)
        std::cerr << "BYTE IS NOT HEX\n\n\n";

    return false;
}

bool byte_from_hex2( char c1, char c2, uint8_t &result )
{
    uint8_t r1, r2;

    if (!byte_from_hex1( c1, r1 )) return false;
    if (!byte_from_hex1( c2, r2 )) return false;

    result = r1*16+r2;

    return true;
}

bool bytes_from_ascii_hex( std::vector<uint8_t>::const_iterator b, const std::vector<uint8_t>::const_iterator e, std::vector<uint8_t> &result )
{
    if ((e-b)%2!=0)
    {
        if (!silent)
            std::cerr << "NOT ASCII\n";
        return false;
    }
    while (b<e)
    {
        uint8_t curr;
        if (!byte_from_hex2( b[0], b[1], curr ))
            return false;
        result.push_back( curr );
        // std::cout << "[" << (int)curr << "]";
        b += 2;
    }

    return true;
}

bool bytes_from_bits( std::vector<bool>::const_iterator b, const std::vector<bool>::const_iterator e, std::vector<uint8_t> &result )
{
    if ((e-b)%16!=0)
    {
        if (!silent)
            std::cerr << "bits not multiples of 16\n";
        return false;
    }

    auto hex = ascii_hex_from_bits( b, e );
    return bytes_from_ascii_hex( std::begin(hex), std::end(hex), result );
}

struct kim_data
{
    uint8_t id;
    uint16_t adrs;
    std::vector<uint8_t> data;
    uint16_t checksum;

    uint16_t compute_checksum()
    {
        uint16_t result = 0;
        for (auto b:data)
            result += b;
        result -= data[0];  //  ID is not in checksum
        return result;
    }

    void dump()
    {
        printf( "ID: %02X LOADED AT: %04X", id, adrs );
        for (int i=3;i!=data.size();i++)
        {
            if ((i%16)==3)
                printf( "\n    " );
            printf( "%02X ", data[i] );
        }
        printf( "\n" );
    }

};

bool kim_data_from_bits( const std::vector<bool> &encoded, kim_data &result )
{
    //  Find first 'on' bit
    auto b = std::begin(encoded);
    auto e = std::end(encoded);

loop:
    //  Lookup for SYN ('00010110')
    b = find_bits( b,e, { false, true, true, false, true, false, false, false } );

    if (b==e)
        return false; //  No on bits left
    
    //  Scan by 8 until zero found
    while (compare_bits( b, e, { false, true, true, false, true, false, false, false } ))
            b += 8;

    //  check for '*' ('00101010')
    if (!compare_bits( b, e, { false, true, false, true, false, true, false, false } ))
        goto loop;  //  evil

    b += 8;

    //  We are at the start of the data

    //  Look for the '/' 0x2f '00101111'
    auto slash = find_bits( b, e, { true, true, true, true, false, true, false, false }, 8 );
    if (slash==e)
    {
        if (!silent)
            std::cerr << "'/ not found\n";
        return false;
    }

    //  Look for the EOF 0x04 '00000100'
    auto eos = find_bits( slash, e, { false, false, true, false, false, false, false, false }, 8 );
    if (eos==e)
    {
        if (!silent)
            std::cerr << "EOS not found\n";
        return false;
    }

    //  Check EOF is 40 bits (5 bytes) after the '/'
    if (eos-slash!=40)
    {
        if (!silent)
            std::cerr << "'/' and EOS not 40 bits apart'\n";
        return false;
    }

    if (!bytes_from_bits( b, slash, result.data ))
    {
        if (!silent)
            std::cout << "Cannot parse content\n";
        return false;
    }

    std::vector<uint8_t> checksum;
    if (!bytes_from_bits( slash+8, eos, checksum ))
    {
        if (!silent)
            std::cout << "Cannot parse checksum\n";
        return false;
    }

    result.id = result.data[0];
    result.adrs = result.data[1]+((uint16_t)result.data[2])*256;
    result.checksum = checksum[0]+((uint16_t)checksum[1])*256;

    // std::cout << "CHK:" << result.compute_checksum() << " vs " << result.checksum << "\n";

    if (result.compute_checksum()!=result.checksum)
    {
        if (!silent)
            std::cout << "Wrong checksum value\n";
        return false;
    }

    // for (auto b:result.data)
    //     std::cout << (int)b << " ";
    // std::cout << "\n";

    return true;
}

void parse( const std::vector<sample_t> data, std::string patch )
{
    Parser p;
    for (auto s:data)
        p.add( s );

    // for (int i=0;i!=8;i++)
    // {
    //     std::cout << "\n\n\n------------------------------------- " << i << "\n\n\n";
    //     std::cout << string_from_bits( p.result, i );
    // }

    // auto start = kim_data_from_bits( p.result );

    auto bs =p.get_bitstream();
    // std::cout << bs.fix_count() << "\n";

        //  we patch according to user specs
    bs.patch( patch );

        //  we interate all the solutions
    std::cout << "Generating " << bs.fix_count() << " combinations\n";
    for (size_t i=0;i!=bs.fix_count();i++)
    {
        auto bits = bs.bits( i );
        kim_data kd;
        if (kim_data_from_bits( bits, kd ))
        {
            std::cout << "Found parsable data with correct checksum:\n";
            kd.dump();
            return ;
        }
    }

// exit(0);

//     if (bs.errors()>0)
//     {
//         std::cout << "Location in bitstream for corrupted segments:\n";
//         for (int i=0;i!=p.fixes.size();i++)
//         {
//             auto f = p.fixes[i];
//             bool bit = patch[i%patch.size()]=='1';
//             std::cout << "  " << from_time( f.timestamp ) << "-" << from_time( f.timestamp+7.452/1000 ) << " -- bit #" << f.bit_location << " inserted " << bit << "\n";
//             p.result[p.fixes[i].bit_location] = bit;
//         }
    // }
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

    test_bitstream();

    std::string patch;

    argc--;
    argv++;

    while (argc)
    {
        if (!strcmp(*argv,"--help"))
        {
            std::cerr << "kimreader [--silent true|false] [--verbose true|false] [--smooth <NUM>] file.wav\n";
            std::cerr << " '*' : got an zero crossing that is not 2400Hz or 3700Hz\n";
            std::cerr << " '?' : got a transition from 2400Hz to 3700Hz that is not in a 9-9-6 or 9-6-6 pattern\n";
            std::cerr << " '#' : inserting an unknown bit\n";
            std::cerr << "       (insertion is only done *after* a first know bit is found, and only if followed by a known bit)\n";
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
        else if (!strcmp(*argv,"--patch"))
        {
            argc--;
            argv++;
            patch = *argv;
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

    parse( norm, patch );

    return EXIT_SUCCESS;
}
