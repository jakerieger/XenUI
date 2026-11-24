cbuffer TextureConstants : register(b0) {
    float2 ViewportSize;
    float SourceOffset;
    float Opacity;
    float4 Destination;
};

Texture2D Source : register(t0);
SamplerState WrapSampler : register(s0);

struct Interpolants {
    float4 Position : SV_Position;
    float2 Coordinate : TEXCOORD0;
};

Interpolants VsMain(uint Corner : SV_VertexID) {
    const float2 Side     = float2(Corner & 1, (Corner >> 1) & 1);
    const float2 Position = Destination.xy + Destination.zw * Side;

    Interpolants Result;
    Result.Position   = float4(Position / ViewportSize * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    Result.Coordinate = float2(Side.x + SourceOffset, Side.y);
    return Result;
}

float4 PsMain(Interpolants Input) : SV_Target {
    return Source.Sample(WrapSampler, Input.Coordinate) * Opacity;
}
