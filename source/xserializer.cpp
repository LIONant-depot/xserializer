
#include "xserializer.h"
#include "source/xcompression.h"
#include <format>

namespace xserializer
{
    template<typename T>
    struct unique_span : std::span<T>
    {
        using std::span<T>::operator[];

        void New( std::size_t Count )
        {
            m_Data = std::make_unique<T[]>(Count);
            static_cast<std::span<T>&>(*this) = std::span<T>{m_Data.get(), Count};
        }

        std::unique_ptr<T[]> m_Data;
    };

    //------------------------------------------------------------------------------
    // Create the compressor capable of compressing both dynamic and fixed size blocks
    //------------------------------------------------------------------------------
    struct compressor
    {
        xcompression::dynamic_block_compress m_DynamicCompress  = {};
        xcompression::fixed_block_compress   m_FixedCompress    = {};
        std::uint64_t                        m_LastPosition     = 0;
        bool                                 m_bUseDynamic      = false;

        xerr Init( std::uint64_t BlockSize, const std::span<const std::byte> SourceUncompress, compression_level CompressionLevel ) noexcept
        {
            xcompression::dynamic_block_compress::level DLevel={};
            xcompression::fixed_block_compress::level   FLevel={};

            switch (CompressionLevel)
            {
            case compression_level::FAST:
                m_bUseDynamic   = false;
                FLevel          = xcompression::fixed_block_compress::level::FAST;
                break;
            case compression_level::LOW:
                m_bUseDynamic   = false;
                FLevel          = xcompression::fixed_block_compress::level::MEDIUM;
                break;
            case compression_level::MEDIUM:
                m_bUseDynamic   = true;
                DLevel          = xcompression::dynamic_block_compress::level::MEDIUM;
                break;
            case compression_level::HIGH:
                m_bUseDynamic   = true;
                DLevel          = xcompression::dynamic_block_compress::level::HIGH;
                break;
            }

            // Initialize
            if (m_bUseDynamic) return m_DynamicCompress.Init( false, BlockSize, SourceUncompress, DLevel );
            else               return m_FixedCompress.Init(false, BlockSize, SourceUncompress, FLevel );
        }

        std::uint64_t getLastPosition() const noexcept
        {
            return m_LastPosition;
        }

        std::uint64_t getPos() const noexcept
        {
            return m_bUseDynamic ? m_DynamicCompress.m_Position : m_FixedCompress.m_Position;
        }

        xerr Pack(std::uint64_t& CompressedSize, std::span<std::byte> Destination ) noexcept
        {
            if (m_bUseDynamic) 
            {
                m_LastPosition = m_DynamicCompress.m_Position;
                return m_DynamicCompress.Pack(CompressedSize, Destination);
            }
            else
            {
                m_LastPosition = m_FixedCompress.m_Position;
                return m_FixedCompress.Pack(CompressedSize, Destination);
            }
        }
    };

    //------------------------------------------------------------------------------

    namespace endian
    {
        constexpr static std::uint8_t Convert(std::uint8_t value) noexcept
        {
            return value;
        }

        constexpr static std::uint16_t Convert(std::uint16_t value) noexcept
        {
            return (value >> 8) | (value << 8);
        }

        constexpr static std::uint32_t Convert(std::uint32_t value) noexcept
        {
            return ((value >> 24) & 0xFF)       | ((value >> 8) & 0xFF00) |
                   ((value <<  8) & 0xFF0000)   |  (value << 24);
        }

        constexpr static std::uint64_t Convert(std::uint64_t value) noexcept
        {
            return  ((value >> 56) & 0xFF)              | ((value >> 40) & 0xFF00)          |
                    ((value >> 24) & 0xFF0000)          | ((value >>  8) & 0xFF000000)      |
                    ((value <<  8) & 0xFF00000000)      | ((value << 24) & 0xFF0000000000)  |
                    ((value << 40) & 0xFF000000000000)  |  (value << 56);
        }
    }



    //------------------------------------------------------------------------------

    stream::stream(const default_memory_hadler& MemoryHandler) noexcept
        : m_MemoryCallback{ MemoryHandler }
    {
    }

    //------------------------------------------------------------------------------

    std::uint32_t stream::writing::AllocatePack( mem_type DefaultPackFlags ) noexcept
    {
        constexpr std::wstring_view Name(L"ram:\\Whatever");

        // Create the default pack
        auto& WPack = m_Packs.emplace_back();
        if( auto Error = WPack.m_Data.open(Name, "wb+"); Error )
        {
            assert(false);
            return ~0;
        }
        WPack.m_PackFlags = DefaultPackFlags;

        return static_cast<std::uint32_t>(m_Packs.size() - 1);
    }

    //------------------------------------------------------------------------------

    xerr stream::HandlePtrDetails( const std::byte* pA, std::size_t SizeofA, std::size_t Count, mem_type MemoryFlags ) noexcept
    {
        // If the parent is in not in a common pool then its children must also not be in a common pool.
        // The theory is that if the parent is not in a common pool it could be deallocated and if the child 
        // is in a common pool it could be left orphan. However this may need to be thought out more carefully
        // so I am playing it safe for now.
        if( m_pWrite->m_Packs[m_iPack].m_PackFlags.m_bUnique )
        {
            assert(MemoryFlags.m_bUnique);
        }
        else if (m_pWrite->m_Packs[m_iPack].m_PackFlags.m_bTempMemory)
        {
            assert(MemoryFlags.m_bTempMemory);
        }

        //
        // If we don't have any elements then just write the pointer raw
        //
        if (Count == 0)
        {
            // always write 64 bits worth (assuming user is using xserialfile::ptr)
            // if we do not do this the upper bits of the 64 bits may contain trash
            // and if the compiler is 32bits and the game 64 then that trash can crash
            // the game.
            if( auto Err = Serialize(*((std::uint64_t*)(pA))); Err )
                return Err;

            return {};
        }

        //
        // Choose the right pack for this allocation        
        //

        // Back up the current pack
        auto BackupPackIndex = m_iPack;

        if( MemoryFlags.m_bUnique )
        {
            // Create a pack
            m_iPack = m_pWrite->AllocatePack(MemoryFlags);
        }
        else
        {
            // Search for a pool which matches our attributes
            std::uint32_t i;
            constexpr static auto non_unique_v = []() consteval { mem_type x {.m_bUnique = false, .m_bTempMemory = true, .m_bVRam = true}; return x; }();
            for (i = 0; i < m_pWrite->m_Packs.size(); i++)
            {
                if( (m_pWrite->m_Packs[i].m_PackFlags.m_Value & non_unique_v.m_Value ) == MemoryFlags.m_Value )
                    break;
            }

            // Could not find a pack with compatible flags so just create a new one
            if (i == m_pWrite->m_Packs.size())
            {
                // Create a pack
                m_iPack = m_pWrite->AllocatePack(MemoryFlags);
            }
            else
            {
                // Set the index to the compatible pack
                m_iPack = i;
            }
        }

        // Make sure we are at the end of the buffer before preallocating
        // I have change the alignment from 4 to 8 because of 64 bits OS.
        // it may help. In the future will be nice if the user could specify the alignment.
        if ( auto Err = getW().SeekEnd(0); Err ) 
            return {Err.m_pMessage};

        if (auto Err = getW().AlignPutC(' ', static_cast<int>(SizeofA) * static_cast<int>(Count), 8, false); Err)
            return {Err.m_pMessage};

        //
        // Store the pointer
        //
        {
            auto& Ref = m_pWrite->m_PointerTable.emplace_back();

            std::size_t Pos;
            if ( auto Err = getW().Tell(Pos); Err ) 
                return {Err.m_pMessage};

            Ref.m_PointingAT        = static_cast<std::uint32_t>(Pos);
            Ref.m_OffsetPack        = BackupPackIndex;
            Ref.m_OffSet            = m_ClassPos + ComputeLocalOffset(pA);
            Ref.m_Count             = static_cast<std::uint32_t>(Count);
            Ref.m_PointingATPack    = m_iPack;

            // We better be at the write spot that we are pointing at 
#ifdef _DEBUG
            {
                assert( getW().Tell(Pos) == false );
                assert( Ref.m_PointingAT == Pos );
            }
#endif
        }

        return {};
    }

    //------------------------------------------------------------------------------

    xerr stream::SaveFile(void) noexcept
    {
        //
        // Go throw all the packs and compress them
        //
        for(std::uint32_t i = 0; i < m_pWrite->m_Packs.size(); i++ )
        {
            std::vector<std::byte>      RawData;
            pack_writing&               Pack = m_pWrite->m_Packs[i];

            {
                std::size_t Length;
                if ( auto Err = Pack.m_Data.getFileLength(Length); Err ) 
                    return {Err.m_pMessage};

                assert( Length <= std::numeric_limits<std::uint32_t>::max() );
                Pack.m_UncompressSize = static_cast<std::uint32_t>(Length);
            }

            Pack.m_CompressSize = 0;
            Pack.m_BlockSize    = std::min(max_block_size_v, Pack.m_UncompressSize);

            // Copy the pack into a memory buffer
            RawData.resize(Pack.m_UncompressSize);
            if( auto Err = Pack.m_Data.ToMemory(RawData); Err ) 
                return { Err.m_pMessage };

            //
            // Now compress the memory
            //
            {
                compressor Compress;
                if (auto Err = Compress.Init( Pack.m_BlockSize, RawData, m_CompressionLevel); Err ) 
                    return { Err.m_pMessage };

                // Guess data size assuming worse case number of blocks....
                Pack.m_CompressData.resize(((Pack.m_UncompressSize / Pack.m_BlockSize) + 1) * Pack.m_BlockSize);
                Pack.m_nBlocks = 0;

                while(true)
                {
                    std::uint64_t       CompressedSize;
                    const std::uint64_t ToCompressSize  = std::min(RawData.size() - Compress.getPos(), static_cast<std::uint64_t>(Pack.m_BlockSize));
                    auto                Err             = Compress.Pack(CompressedSize, std::span{ Pack.m_CompressData.data() + Pack.m_CompressSize, ToCompressSize });

                    if(Err)
                    {
                        if (Err.getState<xcompression::state>() == xcompression::state::INCOMPRESSIBLE)
                        {
                            assert( RawData.size() >= (Compress.getLastPosition() + ToCompressSize) );
                            assert(Pack.m_CompressData.size() > Pack.m_CompressSize );

                            memcpy_s( Pack.m_CompressData.data() + Pack.m_CompressSize, Pack.m_CompressData.size() - Pack.m_CompressSize,
                                     RawData.data() + Compress.getLastPosition(), ToCompressSize );

                            m_pWrite->m_CSizeStream.push_back(static_cast<std::uint32_t>(ToCompressSize));
                            Pack.m_CompressSize += static_cast<std::uint32_t>(ToCompressSize);
                            Pack.m_nBlocks++;
                            continue;
                        }
                        else  if (Err.getState<xcompression::state>() != xcompression::state::NOT_DONE)
                        {
                            assert(false);
                            return Err;
                        }
                    }

                    //
                    // Add to the total block if we have data to add
                    //
                    if (CompressedSize > 0 )
                    {
                        m_pWrite->m_CSizeStream.push_back(static_cast<std::uint32_t>(CompressedSize));

                        // Get ready for the next block
                        Pack.m_CompressSize += static_cast<std::uint32_t>(CompressedSize);
                        Pack.m_nBlocks++;
                    }

                    // Check if this was the last block...
                    if (Err == false)
                        break;
                }
            }

            //
            // TODO: Could add a sanity check here and decompress the data and check if everything is OK
            //

            //
            // Close the pack file
            //
            Pack.m_Data.close();
        }

        //
        // Take the references and the packs headers and compress them as well
        //
        std::vector<std::byte>          CompressInfoData;
        std::uint64_t                   CompressInfoDataSize;
        {
            std::vector<std::byte>      InfoData;

            // First update endianess 
            if( m_pWrite->m_bEndian )
            {
                for ( auto& E : m_pWrite->m_PointerTable )
                {
                    E.m_OffSet         = endian::Convert(E.m_OffSet);
                    E.m_Count          = endian::Convert(E.m_Count);
                    E.m_PointingAT     = endian::Convert(E.m_PointingAT);
                    E.m_OffsetPack     = endian::Convert(E.m_OffsetPack);
                    E.m_PointingATPack = endian::Convert(E.m_PointingATPack);
                }

                for( auto& E : m_pWrite->m_Packs )
                {
                    E.m_PackFlags.m_Value   = endian::Convert(E.m_PackFlags.m_Value);
                    E.m_UncompressSize      = endian::Convert(E.m_UncompressSize);
                    E.m_nBlocks             = endian::Convert(E.m_nBlocks);
                }

                for( auto& E : m_pWrite->m_CSizeStream )
                {
                    E = endian::Convert(E);
                }
            }

            // Allocate all the memory that we will need
            InfoData.resize( sizeof(pack)            * m_pWrite->m_Packs.size() 
                           + sizeof(ref)             * m_pWrite->m_PointerTable.size() 
                           + sizeof(std::uint32_t)   * m_pWrite->m_CSizeStream.size() );

            auto pPack          = reinterpret_cast<pack*>           (InfoData.data());
            auto pRef           = reinterpret_cast<ref*>            (&pPack[m_pWrite->m_Packs.size()]);
            auto pBlockSizes    = reinterpret_cast<std::uint32_t*>  (&pRef[m_pWrite->m_PointerTable.size()]);

            CompressInfoData.resize(InfoData.size());

#ifdef _DEBUG
            {
                std::memset( CompressInfoData.data(), 0xBE, CompressInfoData.size() );
            }
#endif

            // Now copy all the info starting with the packs
            for( std::uint32_t i = 0; i < m_pWrite->m_Packs.size(); i++)
            {
                pPack[i] = m_pWrite->m_Packs[i];
            }

            // Now we copy all the references
            for( std::uint32_t i = 0; i < m_pWrite->m_PointerTable.size(); i++)
            {
                pRef[i] = m_pWrite->m_PointerTable[i];
            }

            // Now we copy all the block sizes
            for(std::uint32_t i = 0; i < m_pWrite->m_CSizeStream.size(); i++)
            {
                pBlockSizes[i] = m_pWrite->m_CSizeStream[i];
            }

            // 
            // to compress it
            //
            {
                compressor Compress;
                if ( auto Err = Compress.Init( InfoData.size(), InfoData, m_CompressionLevel); Err ) 
                    return Err;

                if (auto Err = Compress.Pack(CompressInfoDataSize, CompressInfoData); Err)
                {
                    if (Err.getState<xcompression::state>() == xcompression::state::INCOMPRESSIBLE)
                    {
                        memcpy(CompressInfoData.data(), InfoData.data(), InfoData.size() );
                        CompressInfoDataSize = InfoData.size();
                    }
                    else
                    {
                        assert(false);
                        return Err;
                    }
                }

                //
                // TODO: Sanity check, We could uncompressed here if we wanted to...
                //
            }
        }

        //
        // Fill up all the header information
        //
        m_Header.m_SerialFileVersion    = version_id_v;       // Major and minor version ( version pattern helps Identify file format as well)
        m_Header.m_nPacks               = static_cast<std::uint16_t>(m_pWrite->m_Packs.size());
        m_Header.m_nPointers            = static_cast<std::uint16_t>(m_pWrite->m_PointerTable.size());
        m_Header.m_nBlockSizes          = static_cast<std::uint16_t>(m_pWrite->m_CSizeStream.size());
        m_Header.m_SizeOfData           = 0;
        m_Header.m_PackSize             = static_cast<std::uint32_t>(CompressInfoDataSize);
        m_Header.m_AutomaticVersion     = m_ClassSize;

        header  Header;

        if (m_pWrite->m_bEndian)
        {
            Header.m_SerialFileVersion  = endian::Convert(m_Header.m_SerialFileVersion);
            Header.m_PackSize           = endian::Convert(m_Header.m_PackSize);
            Header.m_MaxQualities       = endian::Convert(m_Header.m_MaxQualities);
            Header.m_SizeOfData         = endian::Convert(m_Header.m_SizeOfData);
            Header.m_nPointers          = endian::Convert(m_Header.m_nPointers);
            Header.m_nPacks             = endian::Convert(m_Header.m_nPacks);
            Header.m_nBlockSizes        = endian::Convert(m_Header.m_nBlockSizes);
            Header.m_ResourceVersion    = endian::Convert(m_Header.m_ResourceVersion);
            Header.m_AutomaticVersion   = endian::Convert(m_Header.m_AutomaticVersion);
        }
        else
        {
            Header = m_Header;
        }

        //
        // Save everything into a file
        //
        std::size_t Pos;
        if (auto Err = m_pWrite->m_pFile->Tell(Pos); Err)
            return Err;

        if( auto Err = m_pWrite->m_pFile->WriteSpan(std::span(reinterpret_cast<std::byte*>(&Header), sizeof(Header))); Err ) 
            return Err;

        if( auto Err = m_pWrite->m_pFile->WriteSpan(std::span( CompressInfoData.begin(), CompressInfoData.begin() + CompressInfoDataSize) ); Err ) 
            return Err;

        for( auto& Pack : m_pWrite->m_Packs )
        {
            if( m_pWrite->m_bEndian )
            {
                if( auto Err = m_pWrite->m_pFile->WriteSpan( std::span( Pack.m_CompressData.begin(), Pack.m_CompressData.begin() + endian::Convert(Pack.m_CompressSize))); Err )
                    return Err;
            }
            else
            {
                if( auto Err = m_pWrite->m_pFile->WriteSpan( std::span( Pack.m_CompressData.begin(), Pack.m_CompressData.begin() + Pack.m_CompressSize)); Err )
                    return Err;
            }
        }

        // Write the size of the data
        {
            std::size_t BytePos;
            if ( auto Err = m_pWrite->m_pFile->Tell(BytePos); Err) 
                return Err;

            Pos = BytePos - Pos - sizeof(header);
        }

        Header.m_SizeOfData = static_cast<std::uint32_t>(Pos);
        if (auto Err = m_pWrite->m_pFile->SeekOrigin( offsetof(header, m_SizeOfData) ); Err ) 
            return Err;

        if (m_pWrite->m_bEndian)
        {
            Header.m_SizeOfData = endian::Convert(Header.m_SizeOfData);
        }

        if( auto Err = m_pWrite->m_pFile->Write(Header.m_SizeOfData); Err )
            return Err;

        // Go to the end of the file
        if ( auto Err = m_pWrite->m_pFile->SeekEnd(0); Err ) 
            return Err;

        return {};
    }

    //------------------------------------------------------------------------------

    xerr stream::LoadHeader( xfile::stream& File, std::size_t SizeOfT) noexcept
    {
        //
        // Check signature (version is encoded in signature)
        //
        if ( auto Err = File.ReadSpan(std::span<std::byte>{ reinterpret_cast<std::byte*>(&m_Header), sizeof(m_Header) }); Err )
            return Err;

        if( auto Err = File.Synchronize(true); Err ) 
            return Err;

        if (m_Header.m_SerialFileVersion != version_id_v)
        {
            if ( endian::Convert(m_Header.m_SerialFileVersion) == version_id_v)
            {
                return xerr::create<state::WRONG_VERSION, "File can not be read. Probably it has the wrong endian.">();
            }

            return xerr::create<state::UNKOWN_FILE_TYPE, "Unknown file format (Could be an older version of the file format)">();
        }

        if (m_Header.m_AutomaticVersion != SizeOfT)
        {
            return xerr::create<state::WRONG_VERSION, "The size of the structure that was used for writing this file is different from the one reading it">();
        }

        return {};
    }

    //------------------------------------------------------------------------------

    void* stream::LoadObject( xfile::stream& File ) noexcept
    {
        unique_span<std::byte>           InfoData;                   // Buffer which contains all those arrays
        unique_span<decompress_block>    ReadBuffer;
        std::uint32_t                    iCurrentBuffer = 0;

        //
        // Allocate the read temp double buffer
        //
        ReadBuffer.New(2);

        //
        // Read the refs and packs
        //
        {
            // Create uncompress buffer for the packs and references
            const auto DecompressSize = m_Header.m_nPacks      * sizeof(pack)
                                      + m_Header.m_nPointers   * sizeof(ref)
                                      + m_Header.m_nBlockSizes * sizeof(std::uint32_t);

            assert(m_Header.m_PackSize <= DecompressSize);
            InfoData.New(DecompressSize + m_Header.m_nPacks * sizeof(std::byte * *));

            // Uncompress in place for packs and references
            if ( m_Header.m_PackSize < DecompressSize )
            {
                unique_span<std::byte>  CompressData;

                CompressData.New(m_Header.m_PackSize);

                if ( auto Err = File.ReadSpan(CompressData); Err )
                {
                    xerr::LogMessage<state::FAILURE>( std::format("ERROR:Serializer Load (1) Error {}", Err.getMessage() ) );
                    Err.clear();
                    return nullptr;
                }

                if ( auto Err = File.Synchronize(true); Err )
                {
                    assert(false);
                    return nullptr;
                }

                // Actual decompress the block
                if ( CompressData.size() == DecompressSize )
                {
                    std::ranges::copy( InfoData.begin(), InfoData.end(), CompressData.begin());
                }
                else
                {
                    xcompression::dynamic_block_decompress Decompress;
                    if ( auto Err = Decompress.Init(true, static_cast<std::uint32_t>(DecompressSize)); Err )
                    {
                        assert(false);
                        return nullptr;
                    }

                    std::uint32_t BlockUncompressed=0;
                    if ( auto Err = Decompress.Unpack(BlockUncompressed, InfoData, CompressData); Err )
                    {
                        assert(false);
                        return nullptr;
                    }
                    assert(DecompressSize == BlockUncompressed);
                }
            }
            else
            {
                if ( auto Err = File.ReadSpan(std::span<std::byte>(InfoData.data(), m_Header.m_PackSize)); Err )
                {
                    xerr::LogMessage<state::FAILURE>(std::format("ERROR:Serializer Load (2) Error({})", Err.m_pMessage) );
                    return nullptr;
                }

                // Let as sync before moving forward...
                if ( auto Err = File.Synchronize(true); Err )
                {
                    assert(false);
                }
            }
        }

        //
        // Set up the all the pointers
        //
        auto const   pPack           = reinterpret_cast<const pack*>            (&InfoData[0]);
        auto const   pRef            = reinterpret_cast<const ref*>             (&pPack[m_Header.m_nPacks]);
        auto const   pBlockSizes     = reinterpret_cast<const std::uint32_t*>   (&pRef[m_Header.m_nPointers]);
        auto const   pPackPointers   = const_cast<std::byte**>(reinterpret_cast<const std::byte* const*>(&pBlockSizes[m_Header.m_nBlockSizes]));

        //
        // Start the reading and decompressing of the packs
        //
        {
            std::uint32_t                          iBlock = 0;

            for (std::uint32_t iPack = 0; iPack < m_Header.m_nPacks; iPack++)
            {
                const pack&     Pack        = pPack[iPack];
                std::uint32_t   nBlocks     = 0;
                std::uint32_t   ReadSoFar   = 0;

                xcompression::dynamic_block_decompress Decompress;

                // Initialize the decomporessor
                const auto BlockSize = std::min(max_block_size_v, Pack.m_UncompressSize );
                if (auto Err = Decompress.Init(true, BlockSize); Err)
                {
                    assert(false);
                    return nullptr;
                }

                // Start reading block immediately
                // Note that the rest of the first block of a pack is interleave with the last pack last block
                // except for the very first one (this one)
                if (iPack == 0)
                {
                    if (auto Err = File.ReadSpan(std::span<std::byte>(reinterpret_cast<std::byte*>(&ReadBuffer[iCurrentBuffer]), pBlockSizes[iBlock])); Err)
                    {
                        xerr::LogMessage<state::FAILURE>(std::format("ERROR:Serializer Load (3) Error({})", Err.m_pMessage) );
                        return nullptr;
                    }
                }

                // Allocate the size of this pack
                pPackPointers[iPack] = reinterpret_cast<std::byte*>( m_MemoryCallback.Allocate(Pack.m_PackFlags, Pack.m_UncompressSize, 16 ));

                // Store a block that is mark as temp (can/should only be one)
                if (Pack.m_PackFlags.m_bTempMemory )
                {
                    assert(m_pTempBlockData == nullptr);
                    m_pTempBlockData = pPackPointers[iPack];
                }

                // Read each block
                for ( std::uint32_t i=1; i< Pack.m_nBlocks; ++i )
                {
                    iCurrentBuffer = !iCurrentBuffer;
                    iBlock++;

                    // Start reading the next block
                    if ( auto Err = File.Synchronize(true); Err )
                    {
                        assert(false);
                    }

                    if ( auto Err = File.ReadSpan(std::span{ reinterpret_cast<std::byte*>(&ReadBuffer[iCurrentBuffer]), static_cast<std::size_t>(pBlockSizes[iBlock]) }); Err )
                    {
                        xerr::LogMessage<state::FAILURE>(std::format("ERROR:Serializer Loading block (4) Error({})", Err.getMessage()));
                        assert(false);
                        return nullptr;
                    }

                    //
                    // Start the decompressing at the same time
                    //
                    assert(pPackPointers[iPack]);

                    // Check if the compressor failed to compress the data if so we must just copy the block
                    if( pBlockSizes[iBlock - 1] == BlockSize)
                    {
                        auto dest = std::span<std::byte>{&pPackPointers[iPack][ReadSoFar], Pack.m_UncompressSize - ReadSoFar };
                        auto src  = std::span<std::byte>{reinterpret_cast<std::byte*>(&ReadBuffer[!iCurrentBuffer]), pBlockSizes[iBlock - 1]};
                        std::ranges::copy( src, dest.begin());
                        ReadSoFar += BlockSize;
                    }
                    else
                    {
                        std::uint32_t DecompressSize = 0;
                        auto Err = Decompress.Unpack(DecompressSize
                                        , std::span<std::byte>{&pPackPointers[iPack][ReadSoFar], Pack.m_UncompressSize - ReadSoFar}
                                        , std::span<const std::byte>{reinterpret_cast<const std::byte*>(&ReadBuffer[!iCurrentBuffer]), pBlockSizes[iBlock - 1]});
                        assert( Err == false || Err.getState<xcompression::state>() == xcompression::state::NOT_DONE);
                        ReadSoFar += DecompressSize;
                    }
                }

                // Finish reading the block
                if ( auto Err = File.Synchronize(true); Err )
                {
                    assert(false);
                }

                // Interleave next pack block with this last pack block
                if ((iPack + 1) < m_Header.m_nPacks)
                {
                    if (auto Err = File.ReadSpan(std::span<std::byte>(reinterpret_cast<std::byte*>(&ReadBuffer[!iCurrentBuffer]), pBlockSizes[iBlock + 1])); Err )
                    {
                        xerr::LogMessage<state::FAILURE>( std::format("ERROR:Serializer Load (5) Error({})", Err.m_pMessage) );
                        assert(false);
                        return nullptr;
                    }
                }

                //
                // Decompress last block for this pack
                //
                assert(pPackPointers[iPack]);

                if ( pBlockSizes[iBlock] == BlockSize || Pack.m_UncompressSize == (ReadSoFar + pBlockSizes[iBlock]) )
                {
                    // If we has the same size as the uncompress block means we did not compressed anything
                    std::memcpy(&pPackPointers[iPack][ReadSoFar], &ReadBuffer[iCurrentBuffer], pBlockSizes[iBlock]);
                    ReadSoFar += pBlockSizes[iBlock];
                }
                else
                {
                    std::uint32_t DecompressSize = 0;
                    auto Err = Decompress.Unpack(DecompressSize
                                    , std::span<std::byte>{&pPackPointers[iPack][ReadSoFar], BlockSize }
                                    , std::span<std::byte>{reinterpret_cast<std::byte*>(&ReadBuffer[iCurrentBuffer]), pBlockSizes[iBlock]});
                    assert( Err == false );
                    ReadSoFar += DecompressSize;
                }
                assert(ReadSoFar == Pack.m_UncompressSize);

                //
                // Get ready for next block
                //
                iCurrentBuffer = !iCurrentBuffer;
                iBlock++;
            }
        }

        //
        // Resolve pointers
        //
        for (std::uint32_t i = 0; i < m_Header.m_nPointers; i++)
        {
            const ref&                   Ref       = pRef[i];
            void* const                  pSrcData  = &pPackPointers[Ref.m_PointingATPack][Ref.m_PointingAT];
            xserializer::data_ptr<void>* pDestData = reinterpret_cast<xserializer::data_ptr<void>*>(&pPackPointers[Ref.m_OffsetPack][Ref.m_OffSet]);

            pDestData->m_pValue = pSrcData;
        }

        // Return the basic pack
        return pPackPointers[0];
    }
}