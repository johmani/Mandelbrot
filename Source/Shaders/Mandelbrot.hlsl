struct Parms
{
    double2 offset;    
    double zoom;       
    float4 colors[10];
    uint width;
    uint height;       
    uint maxIterations;
};

ConstantBuffer<Parms> parms : register(b0);
RWTexture2D<float4> output : register(u0);

[numthreads(8, 8, 1)]
void Main(uint3 id : SV_DispatchThreadID)
{
    double2 coord = ((double2(id.xy) + parms.offset) / double2(parms.width, parms.height)) * 2.0 - 1.0;
    coord.x *= double(parms.width) / parms.height;
    double2 c = coord * (1.0 / parms.zoom);
    double2 z = 0;

    uint i = 0;
    for (; i < parms.maxIterations; i++)
    {
        z = double2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
        if ((z.x * z.x + z.y * z.y) > 4.0) break;
    }

    float t = float(i) / parms.maxIterations;
    const uint colorsSize = 10;
    float scaledT = t * (colorsSize - 1);
    uint index = min((uint)scaledT, colorsSize - 1);
    float blendFactor = frac(scaledT);
    float3 color = lerp(parms.colors[index].rgb, parms.colors[index + 1].rgb, blendFactor);

    output[id.xy] = float4(color, 1.0);
}
