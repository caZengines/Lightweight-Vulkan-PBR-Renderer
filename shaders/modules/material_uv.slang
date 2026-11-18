module material_uv;
    // 0 = TEXCOORD_0, 1 = TEXCOORD_1

public[[vk::constant_id(0)]] const uint kBaseColorUvSet = 0;
public[[vk::constant_id(1)]] const uint kNormalUvSet = 1;

public float2 selectUv(uint setIndex, float2 uv0, float2 uv1) {
    return (setIndex == 1) ? uv1 : uv0;
}