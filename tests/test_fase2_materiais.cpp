// tests/test_fase2_materiais.cpp — 0.11-M FASE 2: glTF MATERIAIS.
//
// Pins do dono (a spec da FASE 2 — «15+ texturas + alpha blend +
// texture_transform mostra tudo corretamente»):
//   · o parse COMPLETO: alphaMode MASK/BLEND + alphaCutoff, doubleSided,
//     os QUATRO factors (baseColor/metallic/roughness/emissive)
//   · KHR_texture_transform POR SLOT (offset/rotation/scale) + os samplers
//     do glTF (wrap/filter) → códigos da casa
//   · o bloco GTB2 no .gmesh: round-trip, retro-compat (GTB1-only lê vazio;
//     GTB2 sem binds funciona; CRC mau = erro legível) e o material 100%
//     default NÃO entra (o ficheiro fica byte a byte)
//   · o transform UV puro (a fórmula oficial da extensão) == as colunas
//     do mat2 que o shader recebe
//   · DrawProps::fromGMesh (o slot baseColor → as uniforms do lit)
//   · a ordem do passe BLEND (longe→perto, estável)
//   · u32 índices (componentType 5125) do glTF: parse + conversão
//   · A ACEITAÇÃO: 20 texturas (4 materiais × 5 slots) importam, vinculam
//     e as props chegam ao .gmesh
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>   // getpid (os dirs /tmp por teste)
#include "assets/AssetConverter.h"
#include "assets/GltfImporter.h"
#include "assets/GOwnFormats.h"
#include "core/FsStorage.h"
#include "platform/EngineLog.h"   // elog::init — o log que os pins leem
#include "render/Material.h"      // DrawProps + o transform UV puro
#include "render/BlockMesh.h"     // blendDrawOrder (a ordem do passe BLEND)
#include "render/ThumbPng.h"      // encodePngRgb — PNGs sintéticos distintos

using namespace vv;

namespace {

std::vector<u8> readFixture(const char* name) {
    std::string path = std::string(FIXTURE_DIR) + "/" + name;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        return {};
    }
    std::vector<u8> bytes;
    u8 buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        bytes.insert(bytes.end(), buf, buf + n);
    }
    std::fclose(f);
    return bytes;
}

std::string b64encode(const u8* data, size_t n) {
    static const char* t =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((n + 2) / 3) * 4);
    for (size_t i = 0; i < n; i += 3) {
        const u32 b0 = data[i];
        const u32 b1 = i + 1 < n ? data[i + 1] : 0;
        const u32 b2 = i + 2 < n ? data[i + 2] : 0;
        out += t[b0 >> 2];
        out += t[((b0 & 3) << 4) | (b1 >> 4)];
        out += i + 1 < n ? t[((b1 & 15) << 2) | (b2 >> 6)] : '=';
        out += i + 2 < n ? t[b2 & 63] : '=';
    }
    return out;
}

// PNG sintético DISTINCTO por índice (16×16 RGB, o vermelho varia — o dedup
// por hash exige bytes diferentes para N ficheiros .gtext diferentes)
std::vector<u8> makePngVar(u8 v) {
    std::vector<u8> rgb(16u * 16u * 3u, v);
    return thumb::encodePngRgb(rgb.data(), 16u, 16u);
}

std::vector<u8> triBuffer() {
    std::vector<u8> b(42, 0);
    const float pos[3][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    std::memcpy(b.data(), pos, 36);
    const u16 idx[3] = {0, 1, 2};
    std::memcpy(b.data() + 36, idx, 6);
    return b;
}

// o triângulo com ÍNDICES u32 (componentType 5125 — o mesmo geometry
// buffer, os índices 4 B: 36 + 12 = 48)
std::vector<u8> triBufferU32() {
    std::vector<u8> b(48, 0);
    const float pos[3][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    std::memcpy(b.data(), pos, 36);
    const u32 idx[3] = {0, 1, 2};
    std::memcpy(b.data() + 36, idx, 12);
    return b;
}

std::vector<u8> makeGlb(const std::string& json,
                        const std::vector<u8>& bin) {
    std::vector<u8> jsonBytes(json.begin(), json.end());
    while (jsonBytes.size() % 4u != 0u) {
        jsonBytes.push_back(0x20);
    }
    std::vector<u8> b = bin;
    while (b.size() % 4u != 0u) {
        b.push_back(0);
    }
    const u32 total = 12u + 8u + static_cast<u32>(jsonBytes.size()) + 8u +
                      static_cast<u32>(b.size());
    std::vector<u8> glb;
    const auto u32le = [&glb](u32 v) {
        glb.push_back(static_cast<u8>(v));
        glb.push_back(static_cast<u8>(v >> 8));
        glb.push_back(static_cast<u8>(v >> 16));
        glb.push_back(static_cast<u8>(v >> 24));
    };
    glb.insert(glb.end(), {'g', 'l', 'T', 'F'});
    u32le(2);
    u32le(total);
    u32le(static_cast<u32>(jsonBytes.size()));
    glb.insert(glb.end(), {'J', 'S', 'O', 'N'});
    glb.insert(glb.end(), jsonBytes.begin(), jsonBytes.end());
    u32le(static_cast<u32>(b.size()));
    glb.insert(glb.end(), {'B', 'I', 'N', 0});
    glb.insert(glb.end(), b.begin(), b.end());
    return glb;
}

bool writeSrc(const std::string& path, const std::vector<u8>& bytes) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        return false;
    }
    std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return true;
}

std::string slurp(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        return {};
    }
    std::string s;
    char buf[1024];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        s.append(buf, n);
    }
    std::fclose(f);
    return s;
}

bool nearEqF(f32 a, f32 b, f32 eps = 1e-5f) {
    return std::fabs(a - b) < eps;
}

// glTF de N materiais, CADA um com OS CINCO slots (imagem própria i*5+s).
// `matsJson` = o array de materiais JÁ construído pelo chamador (o helper
// matF2 abaixo) — NUNCA chaves JSON duplicadas (o find do Json devolve a
// PRIMEIRA: uma chave repetida morde o transform silenciosamente).
std::string gltfFase2(u32 nMats, const std::string& matsJson) {
    std::string texs, prims;
    const u32 nImgs = nMats * 5u;
    // samplers 0/1 alternados nas texturas de TODOS os materiais MENOS o
    // último (o pin do «material default não entra no GTB2» precisa de um
    // material 100% default — SEM sampler, SEM alpha, SEM transform)
    const u32 nSamplerImgs = (nMats > 0 ? nMats - 1u : 0u) * 5u;
    for (u32 i = 0; i < nImgs; ++i) {
        texs += (i ? "," : "") + std::string("{\"source\":") +
                std::to_string(i);
        if (i < nSamplerImgs) {
            texs += ",\"sampler\":" + std::to_string(i % 2u);
        }
        texs += "}";
    }
    for (u32 i = 0; i < nMats; ++i) {
        prims += (i ? "," : "") + std::string("{\"attributes\":"
                                              "{\"POSITION\":0},\"indices\":1,"
                                              "\"material\":") +
                  std::to_string(i) + "}";
    }
    return std::string(
               "{\"asset\":{\"version\":\"2.0\"},"
               "\"samplers\":["
               "{\"wrapS\":33071,\"wrapT\":33071,\"magFilter\":9728,"
               "\"minFilter\":9728},"
               "{\"wrapS\":33648,\"wrapT\":10497,\"magFilter\":9729,"
               "\"minFilter\":9987}],"
               "\"buffers\":[{\"byteLength\":42}],"
               "\"bufferViews\":["
               "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
               "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
               "\"accessors\":["
               "{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
               "\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
               "{\"bufferView\":1,\"componentType\":5123,\"count\":3,"
               "\"type\":\"SCALAR\"}],"
               "\"materials\":[" +
               matsJson +
               "],"
               "\"textures\":[" +
               texs +
               "],"
               "\"images\":[],"
               "\"meshes\":[{\"name\":\"tri\",\"primitives\":[" +
               prims +
               "]}],"
               "\"nodes\":[{\"name\":\"raiz\",\"mesh\":0}],"
               "\"scenes\":[{\"nodes\":[0]}]}");
}

// UMA material glTF com TODOS os knobs da FASE 2 (o transform vai DENTRO
// do baseColorTexture do pbr — uma entrada, zero duplicatas)
std::string matF2(u32 i, const char* alphaMode, f32 cutoff, bool cutoffOn,
                  bool twoSided, bool comEmissive, f32 metallic, f32 roughness,
                  bool comFactor, bool comTransform) {
    std::string m = "{\"name\":\"mat" + std::to_string(i) + "\"";
    if (alphaMode != nullptr) {
        m += std::string(",\"alphaMode\":\"") + alphaMode + "\"";
    }
    if (cutoffOn) {
        m += ",\"alphaCutoff\":" + std::to_string(cutoff);
    }
    if (twoSided) {
        m += ",\"doubleSided\":true";
    }
    if (comEmissive) {
        m += ",\"emissiveFactor\":[0.1,0.2,0.3]";
    }
    m += ",\"pbrMetallicRoughness\":{";
    if (comFactor) {
        m += "\"baseColorFactor\":[0.5,0.4,0.3,0.8],";
    }
    if (metallic >= 0.0f) {
        m += "\"metallicFactor\":" + std::to_string(metallic) + ",";
    }
    if (roughness >= 0.0f) {
        m += "\"roughnessFactor\":" + std::to_string(roughness) + ",";
    }
    if (comTransform) {
        m += "\"baseColorTexture\":{\"index\":" +
             std::to_string(i * 5u + 0u) +
             ",\"extensions\":{\"KHR_texture_transform\":"
             "{\"offset\":[0.25,0.5],\"rotation\":1.5707964,"
             "\"scale\":[2,3]}}},";
    } else {
        m += "\"baseColorTexture\":{\"index\":" +
             std::to_string(i * 5u + 0u) + "},";
    }
    m += "\"metallicRoughnessTexture\":{\"index\":" +
         std::to_string(i * 5u + 2u) + "}}";
    m += ",\"normalTexture\":{\"index\":" + std::to_string(i * 5u + 1u) +
         "}";
    m += ",\"emissiveTexture\":{\"index\":" +
         std::to_string(i * 5u + 3u) + "}";
    m += ",\"occlusionTexture\":{\"index\":" +
         std::to_string(i * 5u + 4u) + "}";
    m += "}";
    return m;
}

// props simples para o pin do parse (1 material): MASK + os 4 factors
std::string gltfParseProps() {
    return std::string(
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":42}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
        "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
        "\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
        "{\"bufferView\":1,\"componentType\":5123,\"count\":3,"
        "\"type\":\"SCALAR\"}],"
        "\"materials\":[{"
        "\"name\":\"m0\","
        "\"alphaMode\":\"MASK\",\"alphaCutoff\":0.25,\"doubleSided\":true,"
        "\"emissiveFactor\":[0.1,0.2,0.3],"
        "\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.5,0.4,0.3,0.8],"
        "\"metallicFactor\":0.25,\"roughnessFactor\":0.75}},"
        "{\"name\":\"m1\"},"
        "{\"name\":\"m2\",\"alphaMode\":\"BLEND\"}"
        "],"
        "\"meshes\":[{\"name\":\"tri\",\"primitives\":["
        "{\"attributes\":{\"POSITION\":0},\"indices\":1,\"material\":0}]}],"
        "\"nodes\":[{\"name\":\"raiz\",\"mesh\":0}],"
        "\"scenes\":[{\"nodes\":[0]}]}");
}

// glTF com KHR_texture_transform em DOIS slots + samplers (1 material)
std::string gltfTransformSamplers() {
    return std::string(
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"samplers\":["
        "{\"wrapS\":33071,\"wrapT\":33071,\"magFilter\":9728,"
        "\"minFilter\":9728},"
        "{\"wrapS\":33648,\"wrapT\":10497,\"magFilter\":9729,"
        "\"minFilter\":9987},"
        "{\"wrapS\":10497,\"wrapT\":10497}],"
        "\"buffers\":[{\"byteLength\":42}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
        "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
        "\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
        "{\"bufferView\":1,\"componentType\":5123,\"count\":3,"
        "\"type\":\"SCALAR\"}],"
        "\"textures\":[{\"source\":0,\"sampler\":1},"
        "{\"source\":1,\"sampler\":0}],"
        "\"materials\":[{"
        "\"name\":\"mt\","
        "\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0,"
        "\"extensions\":{\"KHR_texture_transform\":"
        "{\"offset\":[0.25,0.5],\"rotation\":1.5707964,\"scale\":[2,3]}}}},"
        "\"normalTexture\":{\"index\":1,\"extensions\":"
        "{\"KHR_texture_transform\":{\"offset\":[0.1,0.2]}}}}"
        "],"
        "\"meshes\":[{\"name\":\"tri\",\"primitives\":["
        "{\"attributes\":{\"POSITION\":0},\"indices\":1,\"material\":0}]}],"
        "\"nodes\":[{\"name\":\"raiz\",\"mesh\":0}],"
        "\"scenes\":[{\"nodes\":[0]}]}");
}

// glTF com ÍNDICES u32 (componentType 5125)
std::string gltfU32Indices() {
    return std::string(
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":48}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
        "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":12}],"
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
        "\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
        "{\"bufferView\":1,\"componentType\":5125,\"count\":3,"
        "\"type\":\"SCALAR\"}],"
        "\"meshes\":[{\"name\":\"tri\",\"primitives\":["
        "{\"attributes\":{\"POSITION\":0},\"indices\":1}]}],"
        "\"nodes\":[{\"name\":\"raiz\",\"mesh\":0}],"
        "\"scenes\":[{\"nodes\":[0]}]}");
}

std::string gltfComImagens(const std::string& jsonNoImgs,
                           const std::vector<std::vector<u8>>& pngs) {
    std::string imgs;
    for (size_t i = 0; i < pngs.size(); ++i) {
        imgs += (i ? "," : "");
        imgs += "{\"uri\":\"data:image/png;base64," +
                b64encode(pngs[i].data(), pngs[i].size()) + "\"}";
    }
    const size_t pos = jsonNoImgs.find("\"images\":[]");
    return jsonNoImgs.substr(0, pos) + "\"images\":[" + imgs + "]" +
           jsonNoImgs.substr(pos + std::strlen("\"images\":[]"));
}

u32 contaSlots(const std::vector<GMeshMatBind>& binds) {
    u32 n = 0;
    for (const GMeshMatBind& b : binds) {
        for (u8 s = 0; s < kBindSlotCount; ++s) {
            if (!b.tex[s].empty()) {
                ++n;
            }
        }
    }
    return n;
}

} // namespace

// ---- 1. O PARSE: alphaMode/cutoff/doubleSided + os QUATRO factors ---------
TEST(fase2_parse_props_alpha_e_factors) {
    const std::vector<u8> glb = makeGlb(gltfParseProps(), triBuffer());
    GltfModel model;
    std::string err;
    ASSERT(parseGlb(glb.data(), glb.size(), GltfBufferResolver{}, model, err));
    ASSERT(model.materials.size() == 3u);
    const GltfMaterial& m0 = model.materials[0];
    EXPECT(m0.alphaMode == 1);                 // MASK
    EXPECT(nearEqF(m0.alphaCutoff, 0.25f));    // o cutoff lido
    EXPECT(m0.doubleSided);
    EXPECT(nearEqF(m0.metallic, 0.25f));
    EXPECT(nearEqF(m0.roughness, 0.75f));
    EXPECT(nearEqF(m0.emissive[0], 0.1f) && nearEqF(m0.emissive[1], 0.2f) &&
           nearEqF(m0.emissive[2], 0.3f));
    // o baseColorFactor (o factor de SEMPRE — o par dos outros quatro)
    EXPECT(nearEqF(m0.baseColor[0], 0.5f) && nearEqF(m0.baseColor[1], 0.4f) &&
           nearEqF(m0.baseColor[2], 0.3f) && nearEqF(m0.baseColor[3], 0.8f));
    // m1 SEM nada: os DEFAULTS do glTF (o pin do default)
    const GltfMaterial& m1 = model.materials[1];
    EXPECT(m1.alphaMode == 0 && m1.alphaCutoff == 0.5f);
    EXPECT(!m1.doubleSided && m1.metallic == 1.0f && m1.roughness == 1.0f);
    EXPECT(m1.emissive[0] == 0.0f && m1.emissive[1] == 0.0f &&
           m1.emissive[2] == 0.0f);
    // m2: BLEND (o segundo modo — o terceiro é o OPAQUE do m1)
    EXPECT(model.materials[2].alphaMode == 2);
}

// ---- 2. O PARSE: o transform POR SLOT + os samplers ------------------------
TEST(fase2_parse_transform_e_sampler_por_slot) {
    const std::vector<u8> glb =
        makeGlb(gltfTransformSamplers(), triBuffer());
    GltfModel model;
    std::string err;
    ASSERT(parseGlb(glb.data(), glb.size(), GltfBufferResolver{}, model, err));
    ASSERT(model.materials.size() == 1u);
    ASSERT(model.samplers.size() == 3u);
    // os samplers RAW do glTF (o transporte honesto — o GL mapeia no device)
    EXPECT(model.samplers[0].wrapS == 33071 && model.samplers[0].wrapT == 33071);
    EXPECT(model.samplers[0].magFilter == 9728);
    EXPECT(model.samplers[1].wrapS == 33648 && model.samplers[1].wrapT == 10497);
    EXPECT(model.samplers[1].minFilter == 9987);
    EXPECT(model.samplers[2].magFilter == -1);   // sem default obrigatório
    const GltfMaterial& m = model.materials[0];
    // o transform do baseColor (slot 0) — a fórmula vai ao shader
    EXPECT(nearEqF(m.texOffset[0][0], 0.25f) &&
           nearEqF(m.texOffset[0][1], 0.5f));
    EXPECT(nearEqF(m.texRotation[0], 1.5707964f));
    EXPECT(nearEqF(m.texScale[0][0], 2.0f) && nearEqF(m.texScale[0][1], 3.0f));
    // o transform do normal (slot 1): SÓ o offset (rot/scale defaults)
    EXPECT(nearEqF(m.texOffset[1][0], 0.1f) &&
           nearEqF(m.texOffset[1][1], 0.2f));
    EXPECT(m.texRotation[1] == 0.0f);
    EXPECT(m.texScale[1][0] == 1.0f && m.texScale[1][1] == 1.0f);
    // o sampler POR SLOT: baseColor usa a textura 0 (sampler 1), normal a
    // textura 1 (sampler 0); os slots sem textura ficam -1
    EXPECT(m.texSampler[0] == 1);
    EXPECT(m.texSampler[1] == 0);
    EXPECT(m.texSampler[2] == -1 && m.texSampler[3] == -1 &&
           m.texSampler[4] == -1);
}

// ---- 3. O MAPEAMENTO puro dos samplers (glTF → códigos da casa) -----------
TEST(fase2_sampler_codes_puro) {
    u8 c[4] = {};
    // REPEAT + LINEAR/MIPMAP = o default da casa (código 0)
    ASSERT(gltfSamplerToCodes(10497, 10497, 9987, 9729, c));
    EXPECT(c[0] == kSamplerDefault && c[1] == kSamplerDefault);
    EXPECT(c[2] == kSamplerLinearMipmapLinear);
    EXPECT(c[3] == kSamplerLinear);
    // CLAMP_TO_EDGE + NEAREST (o par do HUD/foliage)
    ASSERT(gltfSamplerToCodes(33071, 33071, 9728, 9728, c));
    EXPECT(c[0] == kSamplerWrapClamp && c[1] == kSamplerWrapClamp);
    EXPECT(c[2] == kSamplerNearest && c[3] == kSamplerNearest);
    // MIRRORED + LINEAR (min com mip explícito)
    ASSERT(gltfSamplerToCodes(33648, 10497, 9986, 9729, c));
    EXPECT(c[0] == kSamplerWrapMirrored && c[1] == kSamplerDefault);
    EXPECT(c[2] == kSamplerNearestMipmapLinear);
    // o sampler SEM filtros (-1): os defaults entram, a função diz ok
    ASSERT(gltfSamplerToCodes(10497, 10497, -1, -1, c));
    EXPECT(c[0] == 0 && c[1] == 0 && c[2] == 0 && c[3] == 0);
    // o enum DESCONHECIDO: default honesto + false (o chamador loga)
    EXPECT(!gltfSamplerToCodes(12345, 10497, 9987, 9729, c));
    EXPECT(c[0] == kSamplerDefault);
    EXPECT(!gltfSamplerToCodes(10497, 10497, 9984, 9728, c));
    // o MIP filter no MAG (o glTF não o define aí): default + desconhecido
    EXPECT(!gltfSamplerToCodes(10497, 10497, 9728, 9987, c));
    EXPECT(c[3] == kSamplerDefault);
}

// ---- 4. O GTB2: round-trip no import REAL (o BLEND + o transform chegam) --
TEST(fase2_gtb2_roundtrip_no_import) {
    const std::string base = std::string("/tmp/goni_f2a_") +
                             std::to_string(static_cast<long>(::getpid()));
    system(("mkdir -p " + base + "/proj " + base + "/logs").c_str());
    // O PNG PEQUENO da casa (16×16 sintético — o green4096 com 10 cópias
    // em data: URI enchia o disco da sandbox a 642 MB/run — a lição do
    // flake de DISCO documentado; o pipeline processa igual)
    const std::vector<u8> png = makePngVar(200);
    ASSERT(png.size() > 8u);
    // 2 materiais: mat0 COM props (BLEND/two-sided/factors/transform),
    // mat1 default (o pin de dentro da aceitação)
    const std::string mats =
        matF2(0u, "BLEND", 0.5f, false, true, true, -1.0f, -1.0f, true,
              true) +
        "," + matF2(1u, nullptr, 0.5f, false, false, false, -1.0f, -1.0f,
                    false, false);
    const std::vector<u8> glb = makeGlb(
        gltfComImagens(gltfFase2(2, mats), {png, png, png, png, png,
                                            png, png, png, png, png}),
        triBuffer());
    const std::string src = base + "/modelo.glb";
    ASSERT(writeSrc(src, glb));
    elog::init((base + "/logs").c_str());
    FsStorage st((base + "/proj").c_str());
    convert::Output out;
    convert::Stats stats;
    std::string err;
    ASSERT(convert::importFile(src, "modelo.glb", st, nullptr, out, stats,
                               err));
    elog::shutdown();
    ASSERT(out.meshes.size() == 1u);
    std::vector<u8> bytes;
    ASSERT(st.readBytes(out.meshes[0], bytes));
    GMeshV3Meta meta;
    std::vector<GMeshV3Block> blocks;
    std::vector<std::string> names;
    std::vector<GMeshMatBind> binds;
    std::vector<GMeshMatProps> props;
    ASSERT(readGMeshV3Meta(bytes.data(), bytes.size(), meta, blocks, names,
                           err, &binds, &props));
    ASSERT(binds.size() == 2u);   // o GTB1 INTACTO (os 2 materiais)
    EXPECT(contaSlots(binds) == 10u);   // 2 materiais × 5 slots
    // O PIN: o GTB2 chegou com o material 0 (o 1 não entra — default)
    ASSERT(props.size() == 1u);
    EXPECT(props[0].materialIndex == 0u);
    EXPECT(props[0].alphaMode == 2);          // BLEND
    EXPECT(props[0].doubleSided == 1);
    EXPECT(nearEqF(props[0].alphaCutoff, 0.5f));
    EXPECT(nearEqF(props[0].baseColor[0], 0.5f) &&
           nearEqF(props[0].baseColor[1], 0.4f) &&
           nearEqF(props[0].baseColor[2], 0.3f) &&
           nearEqF(props[0].baseColor[3], 0.8f));
    EXPECT(nearEqF(props[0].emissive[0], 0.1f) &&
           nearEqF(props[0].emissive[1], 0.2f) &&
           nearEqF(props[0].emissive[2], 0.3f));
    // o transform UV do slot baseColor sobreviveu ao ficheiro
    EXPECT(nearEqF(props[0].uvOffset[0][0], 0.25f) &&
           nearEqF(props[0].uvOffset[0][1], 0.5f));
    EXPECT(nearEqF(props[0].uvRotation[0], 1.5707964f));
    EXPECT(nearEqF(props[0].uvScale[0][0], 2.0f) &&
           nearEqF(props[0].uvScale[0][1], 3.0f));
    // o sampler do glTF (a textura 0 usa o sampler 0 = CLAMP/NEAREST)
    EXPECT(props[0].sampler[0][0] == kSamplerWrapClamp);
    EXPECT(props[0].sampler[0][2] == kSamplerNearest);
    // os OUTROS slots do material 0: transform default (a textura i*5+1
    // usa o sampler 1 = MIRRORED — o pin do sampler por slot)
    EXPECT(props[0].uvRotation[1] == 0.0f);
    EXPECT(props[0].sampler[1][0] == kSamplerWrapMirrored);
}

// ---- 5. O GTB2: retro-compat + o CRC honesto ------------------------------
TEST(fase2_gtb2_retrocompat_e_crc) {
    const std::string base = std::string("/tmp/goni_f2b_") +
                             std::to_string(static_cast<long>(::getpid()));
    system(("mkdir -p " + base).c_str());
    // (a) o ficheiro PRÉ-FASE 2 (binds sem props): o leitor novo lê
    // binds CHEIOS e props VAZIAS (defaults) — byte a byte
    MeshData m = []() {
        MeshData md;
        for (int i = 0; i < 8; ++i) {
            Vertex v;
            v.pos = Vec3((i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f,
                         (i & 4) ? 1.0f : -1.0f);
            v.normal = Vec3(0, 1, 0);
            md.vertices.push_back(v);
        }
        const u16 q[36] = {0,1,2, 0,2,3, 4,6,5, 4,7,6, 0,4,5, 0,5,1,
                           2,6,7, 2,7,3, 0,3,7, 0,7,4, 1,5,6, 1,6,2};
        md.indices.assign(q, q + 36);
        md.groups.push_back({"a", "", 0, 36});
        return md;
    }();
    std::vector<GMeshMatBind> binds;
    {
        GMeshMatBind b;
        b.materialIndex = 0;
        b.tex[kBindBaseColor] = "assets/x.gtext";
        binds.push_back(b);
    }
    std::vector<u8> bytes;
    std::string err;
    ASSERT(writeGMesh(m, bytes, err, &binds));   // SEM props (o pré-FASE 2)
    {
        GMeshV3Meta meta;
        std::vector<GMeshV3Block> blocks;
        std::vector<std::string> names;
        std::vector<GMeshMatBind> rb;
        std::vector<GMeshMatProps> rp;
        ASSERT(readGMeshV3Meta(bytes.data(), bytes.size(), meta, blocks,
                               names, err, &rb, &rp));
        EXPECT(rb.size() == 1u);
        EXPECT(rp.empty());   // O PIN: sem GTB2 = defaults (nunca lixo)
    }
    // (b) o GTB2 SEM binds (material com props e sem textura nenhuma)
    std::vector<GMeshMatProps> props1;
    {
        GMeshMatProps p;
        p.materialIndex = 0;
        p.alphaMode = 1;          // MASK
        p.alphaCutoff = 0.33f;
        p.doubleSided = 1;
        props1.push_back(p);
    }
    std::vector<u8> bytes2;
    ASSERT(writeGMesh(m, bytes2, err, nullptr, &props1));
    {
        GMeshV3Meta meta;
        std::vector<GMeshV3Block> blocks;
        std::vector<std::string> names;
        std::vector<GMeshMatBind> rb;
        std::vector<GMeshMatProps> rp;
        ASSERT(readGMeshV3Meta(bytes2.data(), bytes2.size(), meta, blocks,
                               names, err, &rb, &rp));
        EXPECT(rb.empty());
        ASSERT(rp.size() == 1u);
        EXPECT(rp[0].alphaMode == 1);
        EXPECT(nearEqF(rp[0].alphaCutoff, 0.33f));
        EXPECT(rp[0].doubleSided == 1);
    }
    // (c) o CRC do GTB2 podre: erro LEGÍVEL (o mesmo contrato do GTB1)
    {
        // corrompe UM byte da entrada (depois do header do GTB2)
        std::vector<u8> bad = bytes2;
        const size_t tail = [&]() -> size_t {
            // o GTB2 é a última coisa: procura o magic a partir do fim
            for (size_t i = bad.size(); i-- > 4;) {
                if (bad[i - 4] == 'G' && bad[i - 3] == 'T' &&
                    bad[i - 2] == 'B' && bad[i - 1] == '2') {
                    return i;
                }
            }
            return 0;
        }();
        ASSERT(tail > 0 && tail < bad.size());
        bad[tail + 2] ^= 0xFF;   // um flip dentro da entrada
        GMeshV3Meta meta;
        std::vector<GMeshV3Block> blocks;
        std::vector<std::string> names;
        std::vector<GMeshMatProps> rp;
        EXPECT(!readGMeshV3Meta(bad.data(), bad.size(), meta, blocks, names,
                                err, nullptr, &rp));
        EXPECT(err.find("GTB2") != std::string::npos ||
               err.find("propriedades") != std::string::npos ||
               err.find("CHECKSUM") != std::string::npos);
    }
    // (d) o material 100% default NEM ENTRA no GTB2 (o ficheiro fica
    // byte a byte com o pré-FASE 2 — o tamanho IGUAL ao do writer antigo)
    {
        std::vector<GMeshMatProps> dflt;
        GMeshMatProps p;   // tudo default
        p.materialIndex = 0;
        dflt.push_back(p);
        std::vector<u8> withD, noD;
        ASSERT(writeGMesh(m, withD, err, nullptr, &dflt));
        ASSERT(writeGMesh(m, noD, err));
        EXPECT(withD.size() == noD.size());   // O PIN: byte a byte
    }
    // (e) a cauda do GTB1 lida PELO leitor antigo: os bytes do GTB2 são
    // ignorados (o contrato «o fim pode trazer bytes a mais»)
    {
        std::vector<GMeshMatBind> rb;
        std::string e2;
        size_t consumed = 0;
        ASSERT(gmeshV3ReadBinds(bytes.data(), bytes.size(), rb, e2,
                                &consumed));
        (void)consumed;
        // e o GTB2 puro, isolado, também lê (a rotina de rotina)
        std::vector<u8> propBytes;
        gmeshV3AppendProps(propBytes, props1);
        ASSERT(!propBytes.empty());
        std::vector<GMeshMatProps> rp;
        ASSERT(gmeshV3ReadProps(propBytes.data(), propBytes.size(), rp, e2));
        EXPECT(rp.size() == 1u && rp[0].alphaMode == 1);
    }
}

// ---- 6. O transform UV puro (a fórmula oficial == o mat2 do shader) -------
TEST(fase2_uv_transform_puro) {
    f32 out[2] = {};
    // identidade: uv' = uv
    khrTextureTransformApply(0.25f, 0.75f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, out);
    EXPECT(nearEqF(out[0], 0.25f) && nearEqF(out[1], 0.75f));
    // offset+scale sem rotação
    khrTextureTransformApply(0.5f, 1.0f, 0.0f, 2.0f, 3.0f, 0.25f, 0.5f, out);
    EXPECT(nearEqF(out[0], 1.25f) && nearEqF(out[1], 3.5f));
    // rotação de 90° (o pin do exemplo da spec: (1,0) → (0,1))
    khrTextureTransformApply(1.0f, 0.0f, 1.5707964f, 1.0f, 1.0f, 0.0f, 0.0f,
                             out);
    EXPECT(nearEqF(out[0], 0.0f, 1e-4f) && nearEqF(out[1], 1.0f, 1e-4f));
    // O PIN DO SHADER: as colunas do mat2 produzem O MESMO valor (o GLSL
    // faz uv' = M·uv + t com M column-major [m00,m10, m01,m11])
    const f32 rot = 1.5707964f, sx = 2.0f, sy = 3.0f, tx = 0.25f, ty = 0.5f;
    f32 m2[4];
    khrTextureTransformMat2(rot, sx, sy, m2);
    khrTextureTransformApply(0.3f, 0.7f, rot, sx, sy, tx, ty, out);
    const f32 gu = m2[0] * 0.3f + m2[2] * 0.7f + tx;   // col-major: u' = m00*u + m01*v + tx
    const f32 gv = m2[1] * 0.3f + m2[3] * 0.7f + ty;   //           v' = m10*u + m11*v + ty
    EXPECT(nearEqF(gu, out[0], 1e-4f) && nearEqF(gv, out[1], 1e-4f));
    // a identidade em mat2 (o default do DrawProps — o uv de sempre)
    f32 id[4];
    khrTextureTransformMat2(0.0f, 1.0f, 1.0f, id);
    EXPECT(id[0] == 1.0f && id[1] == 0.0f && id[2] == 0.0f && id[3] == 1.0f);
}

// ---- 7. O DrawProps (o GTB2 → as uniforms do lit) --------------------------
TEST(fase2_drawprops_from_gmesh) {
    GMeshMatProps p;
    p.alphaMode = 2;
    p.alphaCutoff = 0.4f;
    p.doubleSided = 1;
    p.baseColor[0] = 0.5f;
    p.baseColor[3] = 0.8f;
    p.emissive[2] = 0.3f;
    p.uvRotation[0] = 1.5707964f;
    p.uvScale[0][0] = 2.0f;
    p.uvOffset[0][1] = 0.5f;
    const DrawProps d = DrawProps::fromGMesh(p);
    EXPECT(d.alphaMode == 2 && nearEqF(d.alphaCutoff, 0.4f) && d.twoSided);
    EXPECT(nearEqF(d.baseColor[0], 0.5f) && nearEqF(d.baseColor[3], 0.8f));
    EXPECT(nearEqF(d.emissive[2], 0.3f));
    // o mat2 do slot baseColor == o builder puro (a MESMA fonte)
    f32 m2[4];
    khrTextureTransformMat2(1.5707964f, 2.0f, 1.0f, m2);
    for (int i = 0; i < 4; ++i) {
        EXPECT(nearEqF(d.uvRot[i], m2[i], 1e-4f));
    }
    EXPECT(nearEqF(d.uvOff[0], 0.0f) && nearEqF(d.uvOff[1], 0.5f));
    // os DEFAULTS (o nullptr do draw = o byte a byte pré-FASE 2)
    const DrawProps k;
    EXPECT(k.alphaMode == 0 && k.baseColor[0] == 1.0f && !k.twoSided);
    EXPECT(k.uvRot[0] == 1.0f && k.uvRot[1] == 0.0f && k.uvRot[2] == 0.0f &&
           k.uvRot[3] == 1.0f);
}

// ---- 8. A ORDEM do passe BLEND (longe→perto, ESTÁVEL) ----------------------
TEST(fase2_blend_order_puro) {
    // 5 blocos: z = [0.0 (perto), 0.9 (longe), 0.5, 0.9 (empate), -0.2]
    const f32 z[5] = {0.0f, 0.9f, 0.5f, 0.9f, -0.2f};
    u32 ord[5] = {};
    BlockMesh::blendDrawOrder(z, 5, ord);
    // O PIN: o MAIOR z primeiro (o mais longe desenha antes)
    EXPECT(ord[0] == 1u || ord[0] == 3u);   // os dois 0.9 (estável: 1 antes)
    EXPECT(ord[1] == (ord[0] == 1u ? 3u : 1u));
    EXPECT(ord[2] == 2u);                   // 0.5
    EXPECT(ord[3] == 0u);                   // 0.0
    EXPECT(ord[4] == 4u);                   // -0.2 (o mais perto por último)
    // a estabilidade: com z TODOS iguais a ordem é a da tabela
    const f32 eq[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    u32 ord2[4] = {};
    BlockMesh::blendDrawOrder(eq, 4, ord2);
    for (u32 i = 0; i < 4; ++i) {
        EXPECT(ord2[i] == i);
    }
    // 1 bloco / 0 blocos: sem crash, sem mudança
    u32 um[1] = {};
    BlockMesh::blendDrawOrder(z, 1, um);
    EXPECT(um[0] == 0u);
    BlockMesh::blendDrawOrder(z, 0, um);
    // o «atrás da câmara» (z fora do clip): desenha PRIMEIRO (o mais longe)
    const f32 back[2] = {1e9f, 0.2f};
    u32 ord3[2] = {};
    BlockMesh::blendDrawOrder(back, 2, ord3);
    EXPECT(ord3[0] == 0u && ord3[1] == 1u);
}

// ---- 9. OS ÍNDICES u32 do glTF (o «核查» da spec — parse + conversão) ------
TEST(fase2_u32_indices_import) {
    // (a) o parse: o accessor u32 (5125) lê os MESMOS 3 índices
    const std::vector<u8> glb = makeGlb(gltfU32Indices(), triBufferU32());
    GltfModel model;
    std::string err;
    ASSERT(parseGlb(glb.data(), glb.size(), GltfBufferResolver{}, model, err));
    ASSERT(model.meshes.size() == 1u);
    ASSERT(model.meshes[0].indices.size() == 3u);
    EXPECT(model.meshes[0].indices[0] == 0u &&
           model.meshes[0].indices[1] == 1u &&
           model.meshes[0].indices[2] == 2u);
    // (b) a conversão: o triângulo u32 entra no .gmesh (a MESMA geometria)
    const std::string base = std::string("/tmp/goni_f2c_") +
                             std::to_string(static_cast<long>(::getpid()));
    system(("mkdir -p " + base).c_str());
    const std::string src = base + "/u32.glb";
    ASSERT(writeSrc(src, glb));
    FsStorage st((base + "/proj").c_str());
    convert::Output out;
    convert::Stats stats;
    ASSERT(convert::importFile(src, "u32.glb", st, nullptr, out, stats, err));
    ASSERT(out.meshes.size() == 1u);
    std::vector<u8> bytes;
    ASSERT(st.readBytes(out.meshes[0], bytes));
    GMeshV3Meta meta;
    std::vector<GMeshV3Block> blocks;
    std::vector<std::string> names;
    ASSERT(readGMeshV3Meta(bytes.data(), bytes.size(), meta, blocks, names,
                           err));
    EXPECT(meta.vertexCount == 3u && meta.indexCount == 3u);
    // (c) o bloco use32 (o v3 ESCREVE u32 quando o bloco excede o cap u16):
    // round-trip do indexType=1 (a migração do formato JÁ vive no leitor)
    GMeshV3Block blk;
    blk.vertexCount = 3;
    blk.indexCount = 3;
    blk.indexType = 1;   // u32 LOCAL
    blk.materialIndex = 0;
    // monta os dados do bloco: 3 verts (pos+normal+uv = 32 B) + 3 idx u32
    std::vector<u8> data;
    const float pos[3][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    for (int i = 0; i < 3; ++i) {
        data.insert(data.end(), reinterpret_cast<const u8*>(pos[i]),
                    reinterpret_cast<const u8*>(pos[i]) + 12);
        const float nrm[3] = {0, 1, 0};
        data.insert(data.end(), reinterpret_cast<const u8*>(nrm),
                    reinterpret_cast<const u8*>(nrm) + 12);
        const float uv[2] = {0, 0};
        data.insert(data.end(), reinterpret_cast<const u8*>(uv),
                    reinterpret_cast<const u8*>(uv) + 8);
    }
    const u32 idx32[3] = {0, 1, 2};
    data.insert(data.end(), reinterpret_cast<const u8*>(idx32),
                reinterpret_cast<const u8*>(idx32) + 12);
    blk.dataSize = data.size();
    blk.crc32 = gcrc32(data.data(), data.size());
    blk.aabbMin = Vec3{0, 0, 0};
    blk.aabbMax = Vec3{1, 1, 0};
    GMeshV3Meta meta32;   // os defaults do struct (sem v3Reset — interno)
    meta32.blockVertexCap = kGmeshV3BlockVertexCap;
    meta32.vertexCount = 3;
    meta32.indexCount = 3;
    meta32.blockCount = 1;
    meta32.materialCount = 1;
    meta32.aabbMin = Vec3{0, 0, 0};
    meta32.aabbMax = Vec3{1, 1, 0};
    meta32.attrs[0] = GMeshV3Attr{kAttrPosition, kAttrF32, 0, 0, 0};
    meta32.attrs[1] = GMeshV3Attr{kAttrNormal, kAttrF32, 0, 0, 0};
    meta32.attrs[2] = GMeshV3Attr{kAttrUv0, kAttrF32, 0, 0, 0};
    meta32.attrCount = 3;
    MeshData md;
    std::string e2;
    // o materialize POR FAIXA (o caminho do runtime por blocos) com u32
    ASSERT(gmeshV3MaterializeBlock(data.data(), data.size(), meta32, blk, md,
                                   e2));
    EXPECT(md.vertices.size() == 3u && md.indices.size() == 3u);
    EXPECT(md.indices[0] == 0u && md.indices[1] == 1u && md.indices[2] == 2u);
    EXPECT(nearEqF(md.vertices[1].pos.x, 1.0f));
}

// ---- 10. A ACEITAÇÃO da FASE 2: 20 texturas + blend + transform ------------
TEST(fase2_aceitacao_20_texturas) {
    // 4 materiais × 5 slots = 20 texturas (o pior caso da spec «15+»), o
    // material 0 BLEND com transform + two-sided + factor, o 1 MASK 0.25
    // com emissive, o 2 com os factors PBR, o 3 default (o pin de dentro)
    const std::string mats =
        matF2(0u, "BLEND", 0.5f, false, true, false, -1.0f, -1.0f, true,
              true) +
        "," + matF2(1u, "MASK", 0.25f, true, false, true, -1.0f, -1.0f,
                    false, false) +
        "," + matF2(2u, nullptr, 0.5f, false, false, false, 0.25f, 0.75f,
                    false, false) +
        "," + matF2(3u, nullptr, 0.5f, false, false, false, -1.0f, -1.0f,
                    false, false);
    std::vector<std::vector<u8>> pngs;
    for (u32 i = 0; i < 20u; ++i) {
        pngs.push_back(makePngVar(static_cast<u8>(i * 12u + 3u)));
    }
    const std::string base = std::string("/tmp/goni_f2d_") +
                             std::to_string(static_cast<long>(::getpid()));
    system(("mkdir -p " + base + "/proj " + base + "/logs").c_str());
    const std::string src = base + "/aceitacao.glb";
    ASSERT(writeSrc(src, makeGlb(gltfComImagens(gltfFase2(4, mats), pngs),
                                 triBuffer())));
    elog::init((base + "/logs").c_str());
    FsStorage st((base + "/proj").c_str());
    convert::Output out;
    convert::Stats stats;
    std::string err;
    ASSERT(convert::importFile(src, "aceitacao.glb", st, nullptr, out, stats,
                               err));
    elog::shutdown();
    // 20 texturas DISTINTAS escritas (o dedup não colapsa — bytes únicos)
    EXPECT(stats.textures == 20u);
    ASSERT(out.meshes.size() == 1u);
    std::vector<u8> bytes;
    ASSERT(st.readBytes(out.meshes[0], bytes));
    GMeshV3Meta meta;
    std::vector<GMeshV3Block> blocks;
    std::vector<std::string> names;
    std::vector<GMeshMatBind> binds;
    std::vector<GMeshMatProps> props;
    ASSERT(readGMeshV3Meta(bytes.data(), bytes.size(), meta, blocks, names,
                           err, &binds, &props));
    EXPECT(meta.materialCount == 4u);
    // OS 20 SLOTS vinculados (4 materiais × 5)
    EXPECT(binds.size() == 4u);
    EXPECT(contaSlots(binds) == 20u);
    // os ficheiros EXISTEM no projeto (o passe de texturas escreveu os 20)
    u32 existem = 0;
    for (const GMeshMatBind& b : binds) {
        for (u8 s = 0; s < kBindSlotCount; ++s) {
            std::vector<u8> tb;
            if (!b.tex[s].empty() && st.readBytes(b.tex[s], tb) &&
                !tb.empty()) {
                ++existem;
            }
        }
    }
    EXPECT(existem == 20u);
    // AS PROPS: 3 materiais no GTB2 (o mat3 default fica FORA — o pin)
    ASSERT(props.size() == 3u);
    const GMeshMatProps* p0 = nullptr;
    const GMeshMatProps* p1 = nullptr;
    const GMeshMatProps* p2 = nullptr;
    for (const GMeshMatProps& p : props) {
        if (p.materialIndex == 0u) p0 = &p;
        if (p.materialIndex == 1u) p1 = &p;
        if (p.materialIndex == 2u) p2 = &p;
    }
    ASSERT(p0 != nullptr && p1 != nullptr && p2 != nullptr);
    // mat0: BLEND + two-sided + o transform do baseColor no ficheiro
    EXPECT(p0->alphaMode == 2 && p0->doubleSided == 1);
    EXPECT(nearEqF(p0->uvRotation[0], 1.5707964f));
    EXPECT(nearEqF(p0->uvOffset[0][0], 0.25f));
    EXPECT(nearEqF(p0->baseColor[3], 0.8f));
    // mat1: MASK 0.25 + o emissiveFactor
    EXPECT(p1->alphaMode == 1 && nearEqF(p1->alphaCutoff, 0.25f));
    EXPECT(nearEqF(p1->emissive[1], 0.2f));
    // mat2: os factors PBR (o GTB2 transporta o metallic/roughness para o
    // lit PBR da 0.11-L — a fronteira documentada)
    EXPECT(nearEqF(p2->metallic, 0.25f) && nearEqF(p2->roughness, 0.75f));
    // o mat3 NÃO está (100% default — o pin de dentro da aceitação)
    for (const GMeshMatProps& p : props) {
        EXPECT(p.materialIndex != 3u);
    }
    // O LOG: 1 linha por sampler aplicado é do device (o GpuAssets) —
    // AQUI o pin é o do BIND: as linhas «textura: vinculada material»
    const std::string log = slurp(base + "/logs/engine.log");
    EXPECT(log.find("textura: vinculada material 'mat0' slot baseColor") !=
           std::string::npos);
}
