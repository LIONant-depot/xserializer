#ifndef XSERIALIZER_H
#define XSERIALIZER_H
#pragma once

#include <array>
#include <span>
#include <variant>
#include <string>
#include <cassert>
#include <vector>

#include "source/xfile.h"
#include "source/xerr.h"

namespace xserializer
{
    //------------------------------------------------------------------------------
    // Description:
    //     The xserialfile class is design for binary resources files. It is design
    //     to be super fast loading, super memory efficient and very friendly to the user.
    //     The class will save the data in the final format for the machine. The loading 
    //     structure layouts should be identical to the saving structures. That allows the 
    //     class to save and load in place without the need of any loading function from
    //     the user. Note that if we ever move to windows 64bits we will have to solved the 
    //     case where pointers sizes will be different from the consoles. 
    //
    //<P>  One of the requirements is that the user provides a SerializeIO function per structure 
    //     that needs saving. There are certain cases where you can avoid this thought, check the example.
    //     Having the function allows this class to recurse across the hierarchy of the user classes/structures/buffer and arrays.
    //     Ones the class has finish loading there will be only one pointer that it is return which contains a 
    //     the pointer of the main structure. The hold thing will have been allocated as a
    //     single block of memory. This means that the pointer return is the only pointer that
    //     needs to be deleted.
    //
    //<P>  Loading is design to be broken up into 3 stages. The Loading The Header which load minimum information
    //     about resource, load object which loads the file into memory and finally resolve which calls an specific 
    //     constructor which allows the object to deal with any special cases and talking to other systems.
    //     The reason for it is that the constructor will be call. This constructor is a special
    //     one which accepts a xserialfile structure. Here the user may want to register data with 
    //     ingame managers such vram managers, or animation managers, etc. By having that executed in the
    //     main thread relaxes the constrains of thread safety without sacrificing much performance at all.
    //     
    //<P>  There are two kinds of data the can be save/load. Unique and non-unique. When the user selects
    //     pointers to be unique the system will allocate that memory as a separate buffer. Any pointer
    //     which does not specify unique will be group as a single allocation. This allows the system to
    //     be very efficient with memory allocations. Additional to the Unique Flag you can also set the
    //     vram flag. The system recognizes two kinds these two types of ram, but the only things that it does
    //     internally with this flag is to separate non-unique memory into these two groups. When deleting the 
    //     object all memory mark as non-unique doesn't need to be free up but everything mark as unique does
    //     need to be free up by the user. This can happen in the destructor of the main structure. 
    //
    //<P>  There is only 2 types of functions to save your data. Serialize and SerializeEnum. Serialize is use
    //     to safe atomic types as well as arrays and pointers. SerializeEnum is design to work for enumerations.
    //     Note that endian issues will be automatically resolve as long as you use those two functions. If the 
    //     user decides to save a blob of data he will need to deal with endian swamping. Please use the SwapEndian 
    //     function to determine weather the system is swamping endians. When saving the data you don't need to 
    //     worry about saving the data members in order. The class take care of that internally. 
    // 
    //<P>  Dealings with 64 vs 32 bit pointers. For now there will be two sets of crunchers the one set
    //     will compile data with 64 bits this are specifically for the PC. The others will be compiled for
    //     32bits and there for the pointer sizes will remain 32. It is possible however that down the road
    //     we may want to compile crunchers in 64 bits yet output 32 pointers. In this case there are two 
    //     solution and both relay on a macro that looks like this: X_EXPTR( type, name) This macro will have 
    //     to be used when ever the user wants to declare a pointer inside an structure. So in solution
    //     one it that the pointers will remain 64 even in the target machine. The macro will create an 
    //     additional 32bit dummy variable in the structure. In the other solution the macro will contain 
    //     a smart pointer for 64 bits environments. That smart pointer class will have a global array of real
    //     pointers where it will allocate its entries. 
    //
    //<P><B>Physical File layout in disk</B>
    //<CODE>
    //                          +----------------+      <-+
    //                          | File Header    |        | File header is never allocated.
    //                          +----------------+ <-+  <-+
    //                          | BlockSizes +   |   |
    //                          | PointerInfo +  |   |  This is temporary allocated and it gets deleted 
    //                          | PackInfo       |   |  before the LoadObject function returns.
    //                          |                |   |
    //                          +----------------+ <-+  <-+ 
    //                          |                |        | Here are a list of blocks which contain the real
    //                          | Blocks         |        | data that the user saved. Blocks are compress
    //                          |                |        | by the system and decompress at load time.
    //                          |                |        | The system will call a user function to allocate the memory.
    //                          +----------------+      <-+
    //</CODE>
    // Example:
    //------------------------------------------------------------------------------

    //-----------------------------------------------------------------------------------------------------

    template< typename T >
    struct alignas(std::uint64_t) data_ptr
    {
        T* m_pValue;
    };

    union mem_type
    {
        std::uint8_t    m_Value{ 0 };
        struct
        {
            bool    m_bUnique:1;        // -> On  - Unique is memory is that allocated by it self and there for could be free
                                        //    Off - Common memory which can't be freed for the duration of the object.
            bool    m_bTempMemory:1;    // -> TODO: this is memory that will be freed after the object constructor returns
                                        //          However you can overwrite this functionality by taking ownership of the temp pointer.
                                        //          The good thing of using this memory type is that multiple allocations are combine into a single one.
                                        //          This flag will override the UNIQUE and VRAM flags, they are exclusive.
                                        //
            bool    m_bVRam:1;          // -> On  - This memory is to be allocated in vram if the hardware has it.
                                        //    Off - Main system memory.
        };
    };
    static_assert(sizeof(mem_type)==1);

    struct memory_handle_base
    {
        virtual void* Allocate  (mem_type Type, std::size_t Size, std::size_t Alignment)   const noexcept = 0;
        virtual void  Free      (mem_type Type, void* pMemory)                             const noexcept = 0;
    };

    struct default_memory_hadler final : memory_handle_base
    {
        void* Allocate(mem_type Type, std::size_t Size, std::size_t Alignment)  const noexcept override
        {
            if (Type.m_bVRam )
            {
                // VRAM memory
                assert(false);
                return nullptr;
            }
            else
            {
                // CPU memory
                return _aligned_malloc(Size, Alignment);
            }
        }

        void Free(mem_type Type, void* pData) const noexcept override
        {
            if (Type.m_bVRam)
            {
                // VRAM memory
                assert(false);
            }
            else
            {
                // CPU memory
                _aligned_free(pData);
            }
        }
    };

    inline constexpr default_memory_hadler default_memory_handler_v;

    class stream;
    // User should place all their serializing function inside the name space
    // Note that the full name space is:
    // namespace xserializer::io_functions
    // {
    //     template<>
    //     xcore::err SerializeIO<my_class>(xcore::serializer::stream& Stream, const my_class& MyClass ) noexcept { ... }
    // }
    namespace io_functions
    {
        template<typename T_CLASS>
        xerr SerializeIO(xserializer::stream& Stream, const T_CLASS&) noexcept;
    }

    enum class compression_level : std::uint8_t
    { FAST
    , LOW
    , MEDIUM
    , HIGH
    };

    enum class state : std::uint8_t
    { OK
    , FAILURE
    , WRONG_VERSION
    , UNKOWN_FILE_TYPE
    };

    class stream
    {
    public:


                                    stream                      (const default_memory_hadler& MemoryHandler = default_memory_handler_v )                    noexcept;

        template< class T >
        inline      xerr            Save                        ( const std::wstring_view FileName
                                                                , const T&              Object
                                                                , compression_level     Level       = compression_level::MEDIUM
                                                                , mem_type              ObjectFlags = {}
                                                                , bool                  bSwapEndian = false
                                                                )                                                                                           noexcept;
        
        template< class T >
        inline      xerr            Save                        ( xfile::stream&        File
                                                                , const T&              Object
                                                                , compression_level     Level       = compression_level::MEDIUM
                                                                , mem_type              ObjectFlags = {}
                                                                , bool                  bSwapEndian = false
                                                                )                                                                                           noexcept;

        template< class T >
        xerr                        Load                        (xfile::stream& File, T*& pObject)                                                          noexcept;
        template< class T >
        xerr                        Load                        (const std::wstring_view FileName, T*& pObject )                                            noexcept;

        void                        DontFreeTempData            (void)                                                                                      noexcept { m_bFreeTempData = false; }
        void*                       getTempData                 (void)                                                                              const   noexcept { assert(m_bFreeTempData == false);  return m_pTempBlockData; }

    public:

        template< class T >
        inline      xerr            Serialize                   (const T& A)                                                                                noexcept;
        template< class T, typename T_SIZE >
        inline      xerr            Serialize                   ( T*const& pView, T_SIZE Size, mem_type MemoryFlags = {} )                                  noexcept;

        xerr                        LoadHeader                  (xfile::stream& File, std::size_t SizeOfT)                                                  noexcept;
        void*                       LoadObject                  (xfile::stream& File)                                                                       noexcept;
        template< class T >
        void                        ResolveObject               (T*& pObject)                                                                               noexcept;

        void                        setResourceVersion          (std::uint16_t ResourceVersion)                                                             noexcept;
        void                        setSwapEndian               (bool SwapEndian)                                                                           noexcept;

        constexpr   bool            SwapEndian                  (void)                                                                              const   noexcept;
        constexpr   std::uint16_t   getResourceVersion          (void)                                                                              const   noexcept;

    protected:

        static constexpr std::uint32_t  version_id_v        = 1;
        static constexpr std::uint32_t  max_block_size_v    = 1024 * 64;

        // This structure wont save to file
        struct decompress_block
        {
            std::array<std::byte, max_block_size_v > m_Buff;
        };

        // This structure will save to file
        struct ref
        {
            std::uint32_t                       m_PointingAT        {}; // What part of the file is this pointer pointing to
            std::uint32_t                       m_OffSet            {}; // Byte offset where the pointer lives
            std::uint32_t                       m_Count             {}; // Count of entries that this pointer is pointing to
            std::uint16_t                       m_OffsetPack        {}; // Offset pack where the pointer is located
            std::uint16_t                       m_PointingATPack    {}; // Pack location where we are pointing to
        };

        // This structure will save to file
        struct pack
        {
            mem_type                            m_PackFlags         {}; // Flags which tells what type of memory this pack is            
            std::uint32_t                       m_UncompressSize    {}; // How big is this pack uncompress
            std::uint32_t                       m_nBlocks           {}; // Number of blocks needed to compress the pack
        };

        // This structure wont save to file
        struct pack_writing : public pack
        {
            xfile::stream                       m_Data              {}; // raw Data for this block
            std::uint32_t                       m_BlockSize         {}; // size of the block for compressing this pack
            std::uint32_t                       m_CompressSize      {}; // How big is this pack compress
            std::vector<std::byte>              m_CompressData      {}; // Data in compress form
        };

        // This structure wont save to file
        struct writing
        {
            std::uint32_t                       AllocatePack        (mem_type DefaultPackFlags) noexcept;

            std::vector<std::uint32_t>          m_CSizeStream       {}; // a in order List of compress sizes for packs and blocks
            std::vector<ref>                    m_PointerTable      {}; // Table of all the pointer written
            std::vector<pack_writing>           m_Packs             {}; // Free-able memory + VRam/Core
            xfile::stream*                      m_pFile             {};
            bool                                m_bEndian           {};
        };

        // This structure will save to file
        struct header
        {
            std::uint32_t                       m_SizeOfData        {}; // Size of this hold data in disk excluding header
            std::uint16_t                       m_SerialFileVersion {}; // Version generated by this system
            std::uint16_t                       m_PackSize          {}; // Pack size
            std::uint16_t                       m_nPointers         {}; // How big is the table with pointers
            std::uint16_t                       m_nPacks            {}; // How many packs does it contain
            std::uint16_t                       m_nBlockSizes       {}; // How many block sizes do we have
            std::uint16_t                       m_ResourceVersion   {}; // User version of this data
            std::uint16_t                       m_MaxQualities      {}; // Maximum number of qualities for this resource
            std::uint16_t                       m_AutomaticVersion  {}; // The size of the main structure as a simple version of the file
        };

    protected:

                    xerr            SaveFile            (void)                                                                                              noexcept;
        inline      xfile::stream&  getW                (void)                                                                                              noexcept;
//                    file::stream&   getTable            (void)                                                                                      const   noexcept;
        constexpr   bool            isLocalVariable     (const std::byte* pRange)                                                                   const   noexcept;
        constexpr   std::int32_t    ComputeLocalOffset  (const std::byte* pItem)                                                                    const   noexcept;
                    xerr            HandlePtrDetails    (const std::byte* pA, std::size_t SizeofA, std::size_t Count, mem_type MemoryFlags)                 noexcept;
        inline      xerr            Handle              (const std::span<const std::byte> View)                                                             noexcept;

    protected:

        // non stack base variables for writing
        writing*                    m_pWrite            {};             // Static data for writing
        compression_level           m_CompressionLevel  { compression_level::MEDIUM };

        // Stack base variables for writing
        std::uint32_t               m_iPack             {};
        std::uint32_t               m_ClassPos          {};
        mutable std::byte*          m_pClass            {};
        std::uint32_t               m_ClassSize         {};

        // Loading data
        header                      m_Header            {};             // Header of the resource
        const memory_handle_base&   m_MemoryCallback;                   // Callback
        void*                       m_pTempBlockData    { nullptr };    // This is data that was saved with the flag temp_data
        bool                        m_bFreeTempData     { true };
    };
}

#include "implementation/xserializer_inline.h"

#endif