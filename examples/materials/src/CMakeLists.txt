add_executable(materials)

target_link_libraries(materials PRIVATE keptech::keptech)

target_sources(materials
  PRIVATE
     FILE_SET HEADERS
  PRIVATE
    main.cpp
)
