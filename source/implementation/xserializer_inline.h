namespace xserializer
{
    namespace details
    {
        //--------------------------------------------------------------------------------------------
        // Determine if a particular objects supports serialization
        //--------------------------------------------------------------------------------------------
        namespace details
        {
            struct nonesuch
            {
                ~nonesuch() = delete;
                 nonesuch(nonesuch const&) = delete;
                void operator=(nonesuch const&) = delete;
            };

            template <class Default, class AlwaysVoid, template<class...> class Op, class... Args>
            struct detector
            {
                using value_t = std::false_type;
                using type    = Default;
            };

            template <class Default, template<class...> class Op, class... Args>
            struct detector<Default, std::void_t<Op<Args...>>, Op, Args...>
            {
                // Note that std::void_t is a C++17 feature
                using value_t = std::true_type;
                using type    = Op<Args...>;
            };

            template <template<class...> class Op, class... Args>
            using is_detected = typename detector<nonesuch, void, Op, Args...>::value_t;

            template <template<class...> class Op, class... Args>
            using detected_t = typename detector<nonesuch, void, Op, Args...>::type;

            template <class Default, template<class...> class Op, class... Args>
            using detected_or = detector<Default, void, Op, Args...>;

            template <class Expected, template<class...> class Op, class... Args>
            using is_detected_exact = std::is_same<Expected, detected_t<Op, Args...>>;

            template< typename T >
            struct fun_return;

            template< typename T_RET, typename...T_ARGS >
            struct fun_return<T_RET(T_ARGS...) noexcept>
            {
                using type = T_RET;
            };
        }

        template <class Expected, template<class...> class Op, class... Args>
        constexpr bool is_detected_exact_v = details::is_detected_exact<Expected, Op, Args...>::value;

        template<class T>
        using has_serialization = typename details::fun_return<decltype(xserializer::io_functions::SerializeIO<T>)>::type;

        template<class T>
        constexpr static bool has_serialization_v = is_detected_exact_v<xerr, has_serialization, T>;

        //--------------------------------------------------------------------------------------------
        // Determine if a type is an array will return true if it is a C array or an object of type array 
        //--------------------------------------------------------------------------------------------
        namespace details
        {
            template<class T>
            struct is_array :std::is_array<T> {};

            template<class T, std::size_t N>
            struct is_array<std::array<T, N>> : std::true_type {};

            template<class T> struct is_array<T const> : is_array<T> {};
            template<class T> struct is_array<T volatile> : is_array<T> {};
            template<class T> struct is_array<T volatile const> : is_array<T> {};
        }

        template< typename T >
        constexpr static bool is_array_v = details::is_array<T>::value;

        //--------------------------------------------------------------------------------------------
        // Determine if a type is span object
        //--------------------------------------------------------------------------------------------
        namespace details
        {
            template<class T>
            struct is_span : std::false_type {};

            template<class T, std::size_t N>
            struct is_span<std::span<T, N>> : std::true_type {};

            template<class T>
            struct is_span<std::span<T>> : std::true_type {};

            template<class T> struct is_span<T const> : is_span<T> {};
            template<class T> struct is_span<T volatile> : is_span<T> {};
            template<class T> struct is_span<T volatile const> : is_span<T> {};
        }
        template< typename T >
        constexpr static bool is_span_v = details::is_span<T>::value;

        //--------------------------------------------------------------------------------------------
        // Always false (helpful when we want to have the last of a list assert for metaprogramming)
        //--------------------------------------------------------------------------------------------
        namespace details
        {
            template< typename T >
            struct always_false : std::false_type {};
        }
        template< typename T >
        constexpr static bool always_false_v = details::always_false<T>::value;

        //--------------------------------------------------------------------------------------------
        // Removes all attributes and references operators giving the basic underlaying type
        //--------------------------------------------------------------------------------------------
        template< typename T >
        using decay_full_t = std::remove_const_t<std::remove_pointer_t<std::decay_t<T>>>;
    }

    //------------------------------------------------------------------------------

    inline
    xfile::stream& stream::getW(void) noexcept
    {
        assert(m_pWrite);
        return m_pWrite->m_Packs[m_iPack].m_Data;
    }

    //------------------------------------------------------------------------------

    constexpr
    bool stream::isLocalVariable(const std::byte* pRange) const noexcept
    {
        return (pRange >= m_pClass) && (pRange < (m_pClass + m_ClassSize));
    }

    //------------------------------------------------------------------------------

    constexpr
    std::int32_t stream::ComputeLocalOffset(const std::byte* pItem) const noexcept
    {
        assert(isLocalVariable(pItem));
        return static_cast<std::int32_t>(pItem - m_pClass);
    }

    //------------------------------------------------------------------------------

    template< class T > inline
    xerr stream::Save(const std::wstring_view FileName, const T& Object, compression_level CompressionLevel, mem_type ObjectFlags, bool bEndianSwap) noexcept
    {
        // Open the file to make sure we are okay
        xfile::stream     File;

        // open the file...
        if ( auto Err = File.open(FileName, "wb"); Err ) 
            return Err;

        // Call the actual save
        if ( auto Err = Save(File, Object, CompressionLevel, ObjectFlags, bEndianSwap); Err )
            return Err;

        // done with the file
        File.close();

        return {};
    }

    //------------------------------------------------------------------------------

    template< class T > inline
    xerr stream::Save( xfile::stream& File, const T& Object, compression_level CompressionLevel, mem_type ObjectFlags, bool bSwapEndian) noexcept
    {
        //
        // Allocate the writing structure
        //
        std::unique_ptr<writing> Write{ new writing };

        //
        // Set the version as specifies by the user
        //
        setResourceVersion(T::VERSION);

        // Assign it so it is accessible for other functions
        // but we keep the owner
        m_pWrite = Write.get();

        // back up the pointer
        Write->m_pFile = &File;

        //
        // Initialize class members
        //
        m_iPack             = Write->AllocatePack(ObjectFlags);
        m_ClassPos          = 0;
        m_CompressionLevel  = CompressionLevel;
        m_pClass            = const_cast<std::byte*>(reinterpret_cast<const std::byte*>(&Object));
        m_ClassSize         = sizeof(Object);
        Write->m_bEndian = bSwapEndian;

        // Save the initial class
        if ( auto Err = getW().putC(' ', m_ClassSize, true); Err ) 
            return Err;

        // Start the saving 
        if( auto Err = xserializer::io_functions::SerializeIO(*this, Object); Err ) 
            return Err;

        // Save the file
        if ( auto Err = SaveFile(); Err ) 
            return Err;

        // clean up
        m_pWrite = nullptr;

        return {};
    }

    //------------------------------------------------------------------------------

    template< class T > inline
    xerr stream::Serialize(const T& A) noexcept
    {
        // Reading or writing?
        assert(m_pWrite);

        if constexpr (  std::is_integral_v<T>
            ||          std::is_floating_point_v<T>
            ||          std::is_enum_v<T>)
        {
            if ( auto Err = Handle(std::span<const std::byte>{&reinterpret_cast<const std::byte&>(A), sizeof(T)}); Err ) 
                return Err;
        }
        else if constexpr (std::is_array_v<T>)
        {
            // Handle C arrays
            for (int i = 0; i < std::extent_v<T>; ++i) 
            {
                if (auto Err = Serialize(A[i]); Err ) 
                    return Err;
            }
        }
        else if constexpr (details::is_array_v<T> || details::is_span_v<T>)
        {
            static_assert(std::is_object_v<T>);
            for (auto& X : A) 
            {
                if ( auto Err = Serialize(X); Err ) 
                    return Err;
            }
        }
        else if constexpr (details::has_serialization_v<T>)
        {
            static_assert(std::is_object_v<T>);
            static_assert(false == std::is_polymorphic_v<T>);

            if (isLocalVariable(reinterpret_cast<const std::byte*>(&A)))
            {
                //return Handle(std::span{ reinterpret_cast<const std::byte*>(&A), sizeof(T) });
                return xserializer::io_functions::SerializeIO(*this, A);
            }
            else
            {
                // Copy most of the data
                stream File(*this);

                std::size_t Offset;
                if (auto Err = getW().Tell(Offset); Err)
                    return Err;

                File.m_iPack        = m_iPack;
                File.m_ClassPos     = static_cast<std::uint32_t>(Offset);
                File.m_pClass       = &const_cast<std::byte&>(reinterpret_cast<const std::byte&>(A));
                File.m_ClassSize    = sizeof(A);

                if ( auto Err = xserializer::io_functions::SerializeIO(File, A); Err ) 
                    return Err;

                // Go the end of the structure 
                return getW().SeekOrigin( File.m_ClassPos + File.m_ClassSize );
            }
        }
        else if constexpr (std::is_trivially_copyable_v<T>)
        {
            /*
            || std::is_same<T, matrix4>
            || std::is_same<T, vector3>
            || std::is_same<T, vector3d>
            || std::is_same<T, bbox>
            || std::is_same<T, vector2>
            || std::is_same<T, vector4>
            || std::is_same<T, quaternion>
            */
            if ( auto Err = Handle(std::span<const std::byte>{ &reinterpret_cast<const std::byte&>(A), sizeof(T) }); Err ) 
                return Err;
        }
        else
        {
            static_assert(details::always_false_v<T>);
        }

        return {};
    }

    //------------------------------------------------------------------------------

    template< class T, typename T_SIZE  > inline
    xerr stream::Serialize(T* const& pView, T_SIZE Size, mem_type MemoryFlags) noexcept
    {
        if (pView == nullptr)
        {
            assert(Size == 0);
            return {};
        }

        const auto  BackupPackIndex = m_iPack;

        // Handle pointer details
        if ( auto Err = HandlePtrDetails
        (
            reinterpret_cast<const std::byte*>(&pView)
            , sizeof(T)
            , Size
            , MemoryFlags
        ); Err ) return Err;

        //
        // Loop throw all the items
        //
        using of_type_t = details::decay_full_t<decltype(pView[0])>;

        // Short-cut
        if constexpr (std::is_integral_v<of_type_t>
                   || std::is_floating_point_v<of_type_t>
                   || std::is_enum_v<of_type_t>)
        {
            if (isLocalVariable(reinterpret_cast<const std::byte*>(&pView[0])))
            {
                if (auto Err = Handle(std::span<const std::byte>{reinterpret_cast<const std::byte*>(&pView[0]), sizeof(of_type_t)* Size }); Err )
                    return Err;
            }
            else
            {
                // Copy most of the data
                stream File(*this);

                std::size_t Offset;
                if (auto Err = getW().Tell(Offset); Err)
                    return Err;

                const std::span NewView{ reinterpret_cast<const std::byte*>(&pView[0]), sizeof(of_type_t) * Size };

                File.m_iPack        = m_iPack;
                File.m_ClassPos     = static_cast<std::uint32_t>(Offset.m_Value);
                File.m_pClass       = &const_cast<std::byte&>(reinterpret_cast<const std::byte&>(NewView[0]));
                File.m_ClassSize    = static_cast<std::uint32_t>(NewView.size());

                if (auto Err = File.Handle(NewView); Err)
                    return Err;

                // Go the end of the structure 
                if (auto Err = getW().SeekOrigin( File.m_ClassPos + File.m_ClassSize ); Err)
                    return Err;
            }
        }
        else
        {
            for (std::uint64_t i = 0; i < Size; i++)
            {
                if (auto Err = Serialize(pView[i]); Err ) 
                    return Err;

                #ifdef _DEBUG
                {
                    std::size_t X;
                    auto Err = getW().Tell(X);
                    assert(Err == false );
                    assert(static_cast<std::uint64_t>(X) >= i);
                }
                #endif
            }
        }

        //
        // Restore the old pack
        //
        m_iPack = BackupPackIndex;
        return {};
    }

    //------------------------------------------------------------------------------
    inline
    xerr stream::Handle(const std::span<const std::byte> View) noexcept
    {
        assert(View.size() >= 0);

        // If it is not a local variable then you must pass the memory type
        assert(isLocalVariable(View.data()));

        // Make sure that we are at the right offset
        if (auto Err = getW().SeekOrigin( m_ClassPos + ComputeLocalOffset(View.data()) ); Err) 
            return Err;

        // Write the data
        return getW().WriteSpan(View);
    }

    //------------------------------------------------------------------------------

    template< class T > inline
    void stream::ResolveObject(T*& pObject) noexcept
    {
        // Check if we have a virtual function... if we do we must construct
        if constexpr (std::is_polymorphic_v<T>)
        {
            // PLEASE NOTE THAT IF YOU ARE DEFAULTING YOUR VARIABLES TO SOME VALUES YOU
            // WILL HAVE A PROBLEM WITH THIS. SINCE IT WILL CALL THOSE VALUES CLEARNING 
            // ANYTHING IT LOADED
            (void)new(pObject) T{ *this };
        }

        // deal with temp data
        if (m_bFreeTempData && m_pTempBlockData)
        {
            std::destroy_at(m_pTempBlockData);
        }
    }


    //------------------------------------------------------------------------------

    template< class T > inline
    xerr stream::Load(xfile::stream& File, T*& pObject) noexcept
    {
        if ( auto Err = LoadHeader(File, sizeof(*pObject)); Err ) 
            return Err;

        if( getResourceVersion() != T::VERSION )
            return xerr::create<state::WRONG_VERSION, "Wrong resource version">();

        pObject = (T*)LoadObject(File);

        ResolveObject(pObject);
        return {};
    }

    //------------------------------------------------------------------------------

    template< class T > inline
    xerr stream::Load(const std::wstring_view FileName, T*& pObject) noexcept
    {
        xfile::stream File;

        if (auto Err = File.open(FileName, "rb"); Err ) 
            return Err;

        // Load the object
        if (auto Err = Load(File, pObject); Err ) 
            return Err;

        // Close the file
        File.close();

        return {};
    }

    //------------------------------------------------------------------------------
    inline
    void stream::setResourceVersion(std::uint16_t ResourceVersion) noexcept
    {
        m_Header.m_ResourceVersion = ResourceVersion;
    }

    //------------------------------------------------------------------------------
    constexpr
    std::uint16_t stream::getResourceVersion(void) const noexcept
    {
        return m_Header.m_ResourceVersion;
    }

    //------------------------------------------------------------------------------
    constexpr
    bool stream::SwapEndian(void) const noexcept
    {
        return m_pWrite->m_bEndian;
    }
}