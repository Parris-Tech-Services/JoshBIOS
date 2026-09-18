# Preserved Claude boot-stack bundle

The original uploaded archive is split into binary chunks because a single large blob was truncated by the tool transport.

Reconstruct with:

```sh
cat recovery/claude/josh-boot-stack.zip.part* > /tmp/josh-boot-stack.zip
sha256sum /tmp/josh-boot-stack.zip
```

Expected archive:

- size: 33543 bytes
- SHA-256: `85952b3c5abe42b19926d524e10c2a76769bde7ca27d571e843c4507935b456b`

Chunks:

| file | bytes | SHA-256 | Git blob |
|---|---:|---|---|
| part00 | 8000 | 64ad8da4e6912ddd98fcd30b2670a4126b2d59d528934f94a85eb23e858a3975 | 534c87c52f4eb6a94cd9479843187f5f2a30bf9d |
| part01 | 8000 | 0a1d4ddf425178e3852028626decbb99802965b2b841ad3c84a7e7a85b65ac86 | f0a11d732cba6ebe313abf51c52fb12b30c55b27 |
| part02 | 8000 | e05e6bb3966e3fa6916789ca6f6dcdce344eca3c458328cc86d25fe2955e8ccd | eaee3826e3f9f691064ab06a041ba8186975c17b |
| part03 | 8000 | bd300943642bd5f10477944a7fc08b7972045ff7c973fc09c0d8ec08a04812b5 | a12fdb5c0637a85c813cbe42b661b4ed15a72f2d |
| part04 | 1543 | d43f300807bcafe586c43761ff86b58d36ca917b8d5976ae0adfbb915789e72c | fdac927ec4a952ce4991c36024d34c8fb25d74e7 |

These blob IDs were independently compared with local `git hash-object` results before this manifest was committed.
