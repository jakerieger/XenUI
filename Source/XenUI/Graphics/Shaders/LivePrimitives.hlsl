cbuffer FrameConstants : register(b0) {
    float2 ViewportSize;
    float PixelsPerDip;
    float Unused;
};

struct Instance {
    float2 Start : START;
    float2 End : END;
    float4 Color : COLOR;
    float Width : WIDTH;
    float Kind : KIND;
};

struct Interpolants {
    float4 Position : SV_Position;
    float2 Local : LOCAL;
    nointerpolation float2 Start : START;
    nointerpolation float2 End : END;
    nointerpolation float4 Color : COLOR;
    nointerpolation float2 Shape : SHAPE;
};

Interpolants VsMain(Instance Primitive, uint Corner : SV_VertexID) {
    const float Feather = 1.0 / PixelsPerDip;
    const float2 Side   = float2(Corner & 1, (Corner >> 1) & 1);

    float2 Position;
    if (Primitive.Kind < 0.5) {
        const float Radius   = max(Primitive.Width, Feather) * 0.5 + Feather;
        const float2 Delta   = Primitive.End - Primitive.Start;
        const float Length   = length(Delta);
        const float2 Along   = Length > 1e-5 ? Delta / Length : float2(1.0, 0.0);
        const float2 Across  = float2(-Along.y, Along.x);
        const float2 Origin  = Primitive.Start - Along * Radius - Across * Radius;
        Position             = Origin + Along * (Length + Radius * 2.0) * Side.x + Across * (Radius * 2.0) * Side.y;
    } else {
        Position = lerp(Primitive.Start - Feather, Primitive.End + Feather, Side);
    }

    Interpolants Result;
    Result.Position = float4(Position / ViewportSize * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    Result.Local    = Position;
    Result.Start    = Primitive.Start;
    Result.End      = Primitive.End;
    Result.Color    = Primitive.Color;
    Result.Shape    = float2(Primitive.Width, Primitive.Kind);
    return Result;
}

float4 PsMain(Interpolants Input) : SV_Target {
    float Distance;
    float Thinness = 1.0;
    if (Input.Shape.y < 0.5) {
        const float MinimumWidth = 1.0 / PixelsPerDip;
        const float Width        = max(Input.Shape.x, MinimumWidth);
        const float2 FromStart   = Input.Local - Input.Start;
        const float2 Segment     = Input.End - Input.Start;
        const float Along        = saturate(dot(FromStart, Segment) / max(dot(Segment, Segment), 1e-6));
        Distance                 = length(FromStart - Segment * Along) - Width * 0.5;
        Thinness                 = saturate(Input.Shape.x / MinimumWidth);
    } else {
        const float2 Center   = (Input.Start + Input.End) * 0.5;
        const float2 HalfSize = (Input.End - Input.Start) * 0.5;
        const float2 Outside  = abs(Input.Local - Center) - HalfSize;
        Distance              = length(max(Outside, 0.0)) + min(max(Outside.x, Outside.y), 0.0);
    }

    const float Coverage = saturate(0.5 - Distance * PixelsPerDip) * Thinness;
    const float Alpha    = Input.Color.a * Coverage;
    return float4(Input.Color.rgb * Alpha, Alpha);
}
