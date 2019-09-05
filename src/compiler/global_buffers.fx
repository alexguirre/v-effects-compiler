cbuffer rage_matrices : register(b1)
{
    row_major float4x4 gWorld;         // Offset:    0 Size:    64
    row_major float4x4 gWorldView;     // Offset:   64 Size:    64
    row_major float4x4 gWorldViewProj; // Offset:  128 Size:    64
    row_major float4x4 gViewInverse;   // Offset:  192 Size:    64
}
