struct image {
        int length;
        char img<>;
};

program COMPRESS_PROG {
        version COMPRESS_VERS {
                image COMPRESS(string) = 1;
        } = 1;
} = 0x23451111;       
