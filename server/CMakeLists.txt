# server/ la executable chinh.
# Chi link vao "engine" - khong bao gio link truc tiep vao "llama",
# va khong include llama.h o bat ky dau trong thu muc nay.


add_executable(server
    main.cpp
)

target_link_libraries(server
    PRIVATE
        engine
)
