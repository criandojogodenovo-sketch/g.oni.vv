// assets/GltfImporter.cpp — parser glTF 2.0 (F5-C).
//
// Camadas: container GLB → JSON (core/Json) → buffers → accessors →
// primitivas → MeshData + materiais + nós. Toda leitura de memória é
// bounds-checked (byteOffset+byteLength dentro do buffer) — ficheiro
// malicioso/truncado falha com erro, nunca lê fora.
#include "assets/GltfImporter.h"
#include "core/Json.h"
#include <cstring>

namespace vv {

namespace {

constexpr u32 kMagicGltf  = 0x46546C67u;   // 'glTF'
constexpr u32 kChunkJson  = 0x4E4F534Au;   // 'JSON'
constexpr u32 kChunkBin   = 0x004E4942u;   // 'BIN'

// ---- base64 -----------------------------------------------------------------
bool base64Decode(const char* src, size_t len, std::vector<u8>& out) {
    out.clear();
    auto val = [](char c) -> i32 {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    u32 acc = 0;
    int bits = 0;
    for (size_t i = 0; i < len; ++i) {
        const char c = src[i];
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') {
            continue;   // padding/whitespace
        }
        const i32 v = val(c);
        if (v < 0) {
            return false;
        }
        acc = (acc << 6) | static_cast<u32>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<u8>((acc >> bits) & 0xFFu));
        }
    }
    return true;
}

// ---- leitura bounds-checked de um bufferView -------------------------------
struct ViewSpan {
    const u8* data = nullptr;
    size_t byteLength = 0;
    size_t stride = 0;   // 0 = compacto
};

bool resolveView(const std::vector<std::vector<u8>>& buffers, const Json& bv,
                 size_t accessorByteOffset, size_t elemSize, ViewSpan& out) {
    const Json* jbuf = bv.find("buffer");
    if (!jbuf || jbuf->type != Json::Type::Number) {
        return false;
    }
    const size_t bi = static_cast<size_t>(jbuf->number);
    if (bi >= buffers.size()) {
        return false;
    }
    const std::vector<u8>& buf = buffers[bi];
    size_t off = 0;
    if (const Json* o = bv.find("byteOffset"); o && o->type == Json::Type::Number) {
        off = static_cast<size_t>(o->number);
    }
    size_t bLen = 0;
    if (const Json* l = bv.find("byteLength"); l && l->type == Json::Type::Number) {
        bLen = static_cast<size_t>(l->number);
    } else {
        return false;
    }
    if (accessorByteOffset > bLen) {
        return false;
    }
    // stride do VIEW (atributos de vértice podem estar interleaved)
    size_t stride = 0;
    if (const Json* s = bv.find("byteStride"); s && s->type == Json::Type::Number) {
        stride = static_cast<size_t>(s->number);
    }
    if (stride != 0 && stride < elemSize) {
        return false;   // stride menor que o elemento é ilegal
    }
    if (stride == 0) {
        stride = elemSize;
    }
    const size_t total = off + accessorByteOffset + bLen;
    if (total > buf.size()) {
        return false;   // view fora do buffer — recusa, não lê fora
    }
    out.data = buf.data() + off + accessorByteOffset;
    out.byteLength = bLen - accessorByteOffset;
    out.stride = stride;
    return true;
}

size_t componentSize(i32 ct) {
    switch (ct) {
        case 5121: return 1;   // u8
        case 5123: return 2;   // u16
        case 5125: return 4;   // u32
        case 5126: return 4;   // f32
        default:   return 0;
    }
}

size_t componentsOf(const std::string& type) {
    if (type == "SCALAR") return 1;
    if (type == "VEC2")   return 2;
    if (type == "VEC3")   return 3;
    if (type == "VEC4")   return 4;
    return 0;
}

f32 readF32(const u8* p) {
    f32 v;
    std::memcpy(&v, p, 4);
    return v;
}

u32 readU32(const u8* p, i32 ct) {
    switch (ct) {
        case 5121: return *p;
        case 5123: { u16 v; std::memcpy(&v, p, 2); return v; }
        case 5125: { u32 v; std::memcpy(&v, p, 4); return v; }
        default:   return 0;
    }
}

} // namespace

bool decodeBase64(const char* src, size_t len, std::vector<u8>& out) {
    return base64Decode(src, len, out);
}

bool parseGltf(const char* json, size_t len, const std::vector<u8>& bin,
               const GltfBufferResolver& resolver, GltfModel& out,
               std::string& err) {
    out = GltfModel{};
    if (!json || len == 0) {
        err = "glTF: json vazio";
        return false;
    }
    Json doc;
    if (!Json::parse(json, len, doc) || doc.type != Json::Type::Object) {
        err = "glTF: JSON inválido";
        return false;
    }
    const Json* jasset = doc.find("asset");
    if (!jasset || !jasset->find("version")) {
        err = "glTF: sem asset.version";
        return false;
    }

    // ---- buffers ------------------------------------------------------------
    std::vector<std::vector<u8>> buffers;
    if (const Json* jb = doc.find("buffers"); jb && jb->type == Json::Type::Array) {
        for (const Json& b : jb->items) {
            const Json* uri = b.find("uri");
            if (!uri || uri->type != Json::Type::String || uri->string.empty()) {
                // sem URI = buffer do GLB (BIN chunk)
                if (bin.empty()) {
                    err = "glTF: buffer sem URI fora de .glb";
                    return false;
                }
                buffers.push_back(bin);
                continue;
            }
            const std::string& u = uri->string;
            constexpr char kDataPfx[] = "data:application/octet-stream;base64,";
            if (u.compare(0, sizeof(kDataPfx) - 1, kDataPfx) == 0) {
                std::vector<u8> decoded;
                if (!base64Decode(u.c_str() + (sizeof(kDataPfx) - 1),
                                  u.size() - (sizeof(kDataPfx) - 1), decoded)) {
                    err = "glTF: data: URI base64 inválida";
                    return false;
                }
                buffers.push_back(std::move(decoded));
            } else if (resolver.fn) {
                std::vector<u8> ext;
                if (!resolver.fn(resolver.user, u.c_str(), ext) || ext.empty()) {
                    err = "glTF: buffer externo não resolvido: " + u;
                    return false;
                }
                buffers.push_back(std::move(ext));
            } else {
                err = "glTF: buffer externo sem resolver: " + u;
                return false;
            }
        }
    }

    // ---- bufferViews --------------------------------------------------------
    const Json* jviews = doc.find("bufferViews");
    if (jviews && jviews->type != Json::Type::Array) {
        err = "glTF: bufferViews inválido";
        return false;
    }

    // ---- images (F5.1-B: texturas embutidas base64 ou bufferView) ----------
    if (const Json* ji = doc.find("images"); ji && ji->type == Json::Type::Array) {
        for (const Json& im : ji->items) {
            GltfImage gi;
            const Json* uri = im.find("uri");
            if (uri && uri->type == Json::Type::String && !uri->string.empty()) {
                const std::string& u = uri->string;
                constexpr char kPngDataPfx[] = "data:image/png;base64,";
                if (u.compare(0, sizeof(kPngDataPfx) - 1, kPngDataPfx) == 0) {
                    if (!base64Decode(u.c_str() + (sizeof(kPngDataPfx) - 1),
                                      u.size() - (sizeof(kPngDataPfx) - 1),
                                      gi.bytes)) {
                        err = "glTF: data: URI de imagem base64 inválida";
                        return false;
                    }
                    gi.mime = "image/png";
                } else if (u.compare(0, 5, "data:") == 0) {
                    gi.mime = "desconhecido";   // mime não-PNG — ignora (sem falha)
                } else {
                    gi.uriPath = u;             // textura EXTERNA
                }
            } else if (const Json* bv = im.find("bufferView");
                       bv && bv->type == Json::Type::Number) {
                // textura EMBUTIDA no binário (GLB): bytes do bufferView
                if (!jviews) {
                    err = "glTF: imagem sem bufferViews";
                    return false;
                }
                const i32 vi = static_cast<i32>(bv->number);
                if (vi < 0 || vi >= static_cast<i32>(jviews->items.size())) {
                    err = "glTF: bufferView da imagem fora do range";
                    return false;
                }
                ViewSpan span;
                if (!resolveView(buffers, jviews->items[static_cast<size_t>(vi)],
                                 0, 1, span)) {
                    err = "glTF: bufferView da imagem fora do buffer";
                    return false;
                }
                gi.bytes.assign(span.data, span.data + span.byteLength);
                if (const Json* mt = im.find("mimeType");
                    mt && mt->type == Json::Type::String) {
                    gi.mime = mt->string;
                }
            }
            out.images.push_back(std::move(gi));
        }
    }

    // ---- textures (fontes de imagem; F5.1-B) --------------------------------
    std::vector<i32> textureSources;
    if (const Json* jt = doc.find("textures"); jt && jt->type == Json::Type::Array) {
        textureSources.reserve(jt->items.size());
        for (const Json& t : jt->items) {
            i32 src = -1;
            if (const Json* s = t.find("source"); s && s->type == Json::Type::Number) {
                if (s->number >= 0 &&
                    s->number < static_cast<f64>(out.images.size())) {
                    src = static_cast<i32>(s->number);
                }
            }
            textureSources.push_back(src);
        }
    }

    // ---- materials (básico) --------------------------------------------------
    if (const Json* jm = doc.find("materials"); jm && jm->type == Json::Type::Array) {
        for (const Json& m : jm->items) {
            GltfMaterial gm;
            if (const Json* n = m.find("name"); n && n->type == Json::Type::String) {
                gm.name = n->string;
            }
            if (const Json* pbr = m.find("pbrMetallicRoughness")) {
                if (const Json* bc = pbr->find("baseColorFactor");
                    bc && bc->type == Json::Type::Array && bc->items.size() == 4) {
                    for (int i = 0; i < 4; ++i) {
                        gm.baseColor[i] = static_cast<f32>(bc->items[i].number);
                    }
                }
                if (const Json* bt = pbr->find("baseColorTexture");
                    bt && bt->type == Json::Type::Object) {
                    if (const Json* ix = bt->find("index");
                        ix && ix->type == Json::Type::Number &&
                        ix->number >= 0 &&
                        ix->number < static_cast<f64>(textureSources.size())) {
                        gm.baseColorTex =
                            textureSources[static_cast<size_t>(ix->number)];
                    }
                }
            }
            out.materials.push_back(std::move(gm));
        }
    }

    // ---- leitor de accessor (bounds-checked) ---------------------------------
    auto readAccessor = [&](i32 accIdx, std::vector<u8>& rawElems, size_t& elemCount,
                            size_t& compCount, size_t& compSize) -> bool {
        if (!jviews) {
            err = "glTF: accessor sem bufferViews";
            return false;
        }
        const Json* ja = doc.find("accessors");
        if (!ja || accIdx < 0 || accIdx >= static_cast<i32>(ja->items.size())) {
            err = "glTF: accessor fora do range";
            return false;
        }
        const Json& a = ja->items[accIdx];
        const Json* jbv = a.find("bufferView");
        const i32 viewIdx =
            (jbv && jbv->type == Json::Type::Number) ? static_cast<i32>(jbv->number) : -1;
        size_t byteOff = 0;
        if (const Json* bo = a.find("byteOffset"); bo && bo->type == Json::Type::Number) {
            byteOff = static_cast<size_t>(bo->number);
        }
        if (const Json* c = a.find("count"); c && c->type == Json::Type::Number) {
            elemCount = static_cast<size_t>(c->number);
        } else {
            err = "glTF: accessor sem count";
            return false;
        }
        i32 ct = 5126;
        if (const Json* t = a.find("componentType"); t && t->type == Json::Type::Number) {
            ct = static_cast<i32>(t->number);
        }
        compSize = componentSize(ct);
        std::string atype = "SCALAR";
        if (const Json* t = a.find("type"); t && t->type == Json::Type::String) {
            atype = t->string;
        }
        compCount = componentsOf(atype);
        if (compSize == 0 || compCount == 0) {
            err = "glTF: accessor com tipo/componente não suportado";
            return false;
        }
        if (viewIdx < 0) {
            err = "glTF: accessor sem bufferView (sparse não suportado)";
            return false;
        }
        if (viewIdx >= static_cast<i32>(jviews->items.size())) {
            err = "glTF: bufferView fora do range";
            return false;
        }
        const size_t elemSize = compSize * compCount;
        ViewSpan span;
        if (!resolveView(buffers, jviews->items[static_cast<size_t>(viewIdx)],
                         byteOff, elemSize, span)) {
            err = "glTF: bufferView fora do buffer (ficheiro corrompido?)";
            return false;
        }
        rawElems.resize(elemCount * elemSize);
        if (span.stride == elemSize) {
            // compacto: cópia direta
            if (elemCount * elemSize > span.byteLength) {
                err = "glTF: accessor excede o bufferView";
                return false;
            }
            std::memcpy(rawElems.data(), span.data, elemCount * elemSize);
        } else {
            // interleaved: copia elemento a elemento respeitando o stride
            for (size_t e = 0; e < elemCount; ++e) {
                if ((e * span.stride) + elemSize > span.byteLength) {
                    err = "glTF: accessor excede o bufferView (stride)";
                    return false;
                }
                std::memcpy(rawElems.data() + e * elemSize,
                            span.data + e * span.stride, elemSize);
            }
        }
        return true;
    };

    // ---- meshes/primitives ----------------------------------------------------
    const Json* jmeshes = doc.find("meshes");
    if (!jmeshes || jmeshes->type != Json::Type::Array || jmeshes->items.empty()) {
        err = "glTF: sem meshes";
        return false;
    }
    for (const Json& jm : jmeshes->items) {
        MeshData md;
        if (const Json* n = jm.find("name"); n && n->type == Json::Type::String) {
            md.name = n->string;
        }
        i32 meshMatIdx = -1;   // primeiro material das primitivas (F5.1-B)
        const Json* jprims = jm.find("primitives");
        if (!jprims || jprims->type != Json::Type::Array) {
            err = "glTF: mesh sem primitives";
            return false;
        }
        for (size_t pi = 0; pi < jprims->items.size(); ++pi) {
            const Json& jp = jprims->items[pi];
            if (const Json* mode = jp.find("mode");
                mode && mode->type == Json::Type::Number &&
                static_cast<i32>(mode->number) != 4) {
                continue;   // só TRIANGLES (linhas/pontos/tiras = fora do escopo)
            }
            const Json* jattrs = jp.find("attributes");
            if (!jattrs || jattrs->type != Json::Type::Object) {
                err = "glTF: primitiva sem attributes";
                return false;
            }
            std::vector<Vec3> pos;
            std::vector<Vec3> nor;
            std::vector<Vec2> uv;
            const Json* jpos = jattrs->find("POSITION");
            if (!jpos || jpos->type != Json::Type::Number) {
                err = "glTF: primitiva sem POSITION";
                return false;
            }
            {
                std::vector<u8> raw;
                size_t count = 0, cc = 0, cs = 0;
                if (!readAccessor(static_cast<i32>(jpos->number), raw, count, cc, cs) ||
                    cc != 3 || cs != 4) {
                    err = err.empty() ? "glTF: POSITION inválido" : err;
                    return false;
                }
                pos.resize(count);
                std::memcpy(pos.data(), raw.data(), count * sizeof(Vec3));
            }
            if (const Json* jn = jattrs->find("NORMAL");
                jn && jn->type == Json::Type::Number) {
                std::vector<u8> raw;
                size_t count = 0, cc = 0, cs = 0;
                if (!readAccessor(static_cast<i32>(jn->number), raw, count, cc, cs) ||
                    cc != 3 || cs != 4) {
                    err = err.empty() ? "glTF: NORMAL inválido" : err;
                    return false;
                }
                nor.resize(count);
                std::memcpy(nor.data(), raw.data(), count * sizeof(Vec3));
            }
            if (const Json* jt = jattrs->find("TEXCOORD_0");
                jt && jt->type == Json::Type::Number) {
                std::vector<u8> raw;
                size_t count = 0, cc = 0, cs = 0;
                if (!readAccessor(static_cast<i32>(jt->number), raw, count, cc, cs) ||
                    cc != 2 || cs != 4) {
                    err = err.empty() ? "glTF: TEXCOORD_0 inválido" : err;
                    return false;
                }
                uv.resize(count);
                std::memcpy(uv.data(), raw.data(), count * sizeof(Vec2));
            }

            // funde a primitiva no MeshData do mesh (rebase de índices)
            const u16 base = static_cast<u16>(md.vertices.size());
            if (md.vertices.size() + pos.size() > 65536) {
                err = "glTF: mesh fundido excede 65535 vértices (limite u16)";
                return false;
            }
            MeshData::Group grp;
            grp.name = "primitive " + std::to_string(pi);
            if (const Json* jmat = jp.find("material");
                jmat && jmat->type == Json::Type::Number &&
                static_cast<size_t>(jmat->number) < out.materials.size()) {
                grp.material = out.materials[static_cast<size_t>(jmat->number)].name;
                if (meshMatIdx < 0) {
                    meshMatIdx = static_cast<i32>(jmat->number);
                }
            }
            grp.firstIndex = static_cast<u32>(md.indices.size());

            for (size_t v = 0; v < pos.size(); ++v) {
                Vertex vtx;
                vtx.pos = pos[v];
                vtx.normal = v < nor.size() ? nor[v] : Vec3{0.0f, 1.0f, 0.0f};
                vtx.uv = v < uv.size() ? uv[v] : Vec2{0.0f, 0.0f};
                md.vertices.push_back(vtx);
            }

            const Json* jidx = jp.find("indices");
            if (jidx && jidx->type == Json::Type::Number) {
                std::vector<u8> raw;
                size_t count = 0, cc = 0, cs = 0;
                if (!readAccessor(static_cast<i32>(jidx->number), raw, count, cc, cs) ||
                    cc != 1) {
                    err = err.empty() ? "glTF: índices inválidos" : err;
                    return false;
                }
                i32 ct = 5123;
                if (const Json* ja = doc.find("accessors");
                    ja && static_cast<size_t>(jidx->number) < ja->items.size()) {
                    if (const Json* t = ja->items[static_cast<size_t>(jidx->number)]
                                             .find("componentType");
                        t && t->type == Json::Type::Number) {
                        ct = static_cast<i32>(t->number);
                    }
                }
                for (size_t i = 0; i < count; ++i) {
                    const u32 idx = readU32(raw.data() + i * cs, ct);
                    if (idx >= pos.size()) {
                        err = "glTF: índice fora do range da primitiva";
                        return false;
                    }
                    md.indices.push_back(static_cast<u16>(base + idx));
                }
            } else {
                // primitiva NÃO indexada: gera 0..count-1
                for (size_t i = 0; i < pos.size(); ++i) {
                    md.indices.push_back(static_cast<u16>(base + i));
                }
            }
            grp.indexCount =
                static_cast<u32>(md.indices.size()) - grp.firstIndex;
            md.groups.push_back(std::move(grp));
        }
        if (md.ok()) {
            out.meshes.push_back(std::move(md));
            out.meshMaterial.push_back(meshMatIdx);   // alinhado com meshes
        }
    }

    // ---- scene/nodes (hierarquia simples) --------------------------------------
    if (const Json* jnodes = doc.find("nodes"); jnodes && jnodes->type == Json::Type::Array) {
        out.nodes.reserve(jnodes->items.size());
        // mapeia o índice do mesh ORIGINAL → posição em out.meshes (meshes
        // sem primitivas TRIANGLES não entram no modelo)
        std::vector<i32> meshRemap(jmeshes->items.size(), -1);
        {
            size_t kept = 0;
            for (size_t m = 0; m < jmeshes->items.size(); ++m) {
                const Json& jm = jmeshes->items[m];
                bool anyTriangles = false;
                if (const Json* jprims = jm.find("primitives");
                    jprims && jprims->type == Json::Type::Array) {
                    for (const Json& jp : jprims->items) {
                        if (const Json* mode = jp.find("mode");
                            !mode || (mode->type == Json::Type::Number &&
                                      static_cast<i32>(mode->number) == 4)) {
                            anyTriangles = true;
                            break;
                        }
                    }
                }
                meshRemap[m] = anyTriangles ? static_cast<i32>(kept++) : -1;
            }
        }
        for (const Json& jn : jnodes->items) {
            GltfNode gn;
            if (const Json* n = jn.find("name"); n && n->type == Json::Type::String) {
                gn.name = n->string;
            }
            if (const Json* m = jn.find("mesh"); m && m->type == Json::Type::Number) {
                const i32 mi = static_cast<i32>(m->number);
                if (mi >= 0 && mi < static_cast<i32>(meshRemap.size())) {
                    gn.mesh = meshRemap[static_cast<size_t>(mi)];
                }
            }
            if (const Json* t = jn.find("translation");
                t && t->type == Json::Type::Array && t->items.size() == 3) {
                gn.translation = Vec3{static_cast<f32>(t->items[0].number),
                                      static_cast<f32>(t->items[1].number),
                                      static_cast<f32>(t->items[2].number)};
            }
            if (const Json* r = jn.find("rotation");
                r && r->type == Json::Type::Array && r->items.size() == 4) {
                gn.rotation = Quat{static_cast<f32>(r->items[0].number),
                                   static_cast<f32>(r->items[1].number),
                                   static_cast<f32>(r->items[2].number),
                                   static_cast<f32>(r->items[3].number)};
            }
            if (const Json* s = jn.find("scale");
                s && s->type == Json::Type::Array && s->items.size() == 3) {
                gn.scale = Vec3{static_cast<f32>(s->items[0].number),
                                static_cast<f32>(s->items[1].number),
                                static_cast<f32>(s->items[2].number)};
            }
            out.nodes.push_back(std::move(gn));
        }
        // 2ª passada: children → parent (por índice de nó)
        for (size_t i = 0; i < jnodes->items.size(); ++i) {
            const Json* jch = jnodes->items[i].find("children");
            if (!jch || jch->type != Json::Type::Array) {
                continue;
            }
            for (const Json& c : jch->items) {
                if (c.type != Json::Type::Number) {
                    continue;
                }
                const size_t child = static_cast<size_t>(c.number);
                if (child < out.nodes.size() && child != i) {
                    out.nodes[child].parent = static_cast<i32>(i);
                }
            }
        }
    }

    if (!out.ok()) {
        err = err.empty() ? "glTF: nenhum mesh com triângulos" : err;
        return false;
    }

    // ---- animations (0.8.1, F7): channels/samplers → GltfAnimation --------
    // O converter para CLIPS do AnimationPlayer vive em assets/GltfAnim
    // (o parser fica GL-free puro; interpolação LINEAR é a suportada — STEP
    // tolerada como linear, CUBICSPLINE extrai o valor do MEIO [in,val,out]).
    if (const Json* janims = doc.find("animations");
        janims && janims->type == Json::Type::Array) {
        for (size_t ai = 0; ai < janims->items.size(); ++ai) {
            const Json& ja = janims->items[ai];
            GltfAnimation anim;
            if (const Json* n = ja.find("name"); n && n->type == Json::Type::String &&
                !n->string.empty()) {
                anim.name = n->string;
            } else {
                anim.name = "anim " + std::to_string(ai);
            }
            // samplers: input (SCALAR f32) + output (VEC3/VEC4 f32)
            if (const Json* jsam = ja.find("samplers");
                jsam && jsam->type == Json::Type::Array) {
                for (const Json& js : jsam->items) {
                    GltfAnimSampler s;
                    bool ok = true;
                    if (const Json* in = js.find("input");
                        in && in->type == Json::Type::Number) {
                        std::vector<u8> raw;
                        size_t count = 0, cc = 0, cs = 0;
                        if (readAccessor(static_cast<i32>(in->number), raw, count,
                                         cc, cs) &&
                            cc == 1 && cs == 4) {
                            s.times.resize(count);
                            std::memcpy(s.times.data(), raw.data(),
                                        count * sizeof(f32));
                        } else {
                            ok = false;
                        }
                    } else {
                        ok = false;
                    }
                    if (ok) {
                        if (const Json* outj = js.find("output");
                            outj && outj->type == Json::Type::Number) {
                            std::vector<u8> raw;
                            size_t count = 0, cc = 0, cs = 0;
                            if (readAccessor(static_cast<i32>(outj->number), raw,
                                             count, cc, cs) &&
                                (cc == 3 || cc == 4) && cs == 4) {
                                s.components = static_cast<u32>(cc);
                                s.values.resize(count * cc);
                                std::memcpy(s.values.data(), raw.data(),
                                            count * cc * sizeof(f32));
                            } else {
                                ok = false;
                            }
                        } else {
                            ok = false;
                        }
                    }
                    if (ok) {
                        // CUBICSPLINE: [in, valor, out] por key → fica o MEIO
                        std::string interp = "LINEAR";
                        if (const Json* ip = js.find("interpolation");
                            ip && ip->type == Json::Type::String) {
                            interp = ip->string;
                        }
                        if (interp == "CUBICSPLINE" &&
                            s.values.size() == s.times.size() * 3u * s.components) {
                            std::vector<f32> mid(s.times.size() * s.components);
                            for (size_t k = 0; k < s.times.size(); ++k) {
                                std::memcpy(&mid[k * s.components],
                                            &s.values[(k * 3 + 1) * s.components],
                                            s.components * sizeof(f32));
                            }
                            s.values = std::move(mid);
                        }
                        // STEP: tolerada como linear (dívida documentada)
                        anim.samplers.push_back(std::move(s));
                    }
                }
            }
            // channels: (nó, path) → sampler
            if (const Json* jch = ja.find("channels");
                jch && jch->type == Json::Type::Array) {
                for (const Json& jc : jch->items) {
                    GltfAnimChannel ch;
                    if (const Json* js = jc.find("sampler");
                        js && js->type == Json::Type::Number) {
                        ch.sampler = static_cast<i32>(js->number);
                    }
                    const Json* jt = jc.find("target");
                    if (jt && jt->type == Json::Type::Object) {
                        if (const Json* n = jt->find("node");
                            n && n->type == Json::Type::Number) {
                            ch.node = static_cast<i32>(n->number);
                        }
                        if (const Json* p = jt->find("path");
                            p && p->type == Json::Type::String) {
                            if (p->string == "rotation") {
                                ch.path = GltfAnimChannel::Path::Rotation;
                            } else if (p->string == "scale") {
                                ch.path = GltfAnimChannel::Path::Scale;
                            } else {
                                ch.path = GltfAnimChannel::Path::Translation;
                            }
                        }
                    }
                    anim.channels.push_back(std::move(ch));
                }
            }
            if (!anim.channels.empty() && !anim.samplers.empty()) {
                out.animations.push_back(std::move(anim));
            }
        }
    }
    return true;
}

bool parseGlb(const u8* data, size_t len, const GltfBufferResolver& resolver,
              GltfModel& out, std::string& err) {
    out = GltfModel{};
    if (!data || len < 12) {
        err = "GLB: ficheiro demasiado curto";
        return false;
    }
    u32 magic = 0, version = 0, total = 0;
    std::memcpy(&magic, data, 4);
    std::memcpy(&version, data + 4, 4);
    std::memcpy(&total, data + 8, 4);
    if (magic != kMagicGltf) {
        err = "GLB: magic inválido (não é .glb)";
        return false;
    }
    if (version != 2) {
        err = "GLB: versão != 2 não suportada";
        return false;
    }
    if (total > len) {
        err = "GLB: declaração de tamanho excede o ficheiro";
        return false;
    }

    size_t off = 12;
    const u8* jsonPtr = nullptr;
    size_t jsonLen = 0;
    std::vector<u8> bin;
    bool binFromChunk = false;

    while (off + 8 <= len) {
        u32 chunkLen = 0, chunkType = 0;
        std::memcpy(&chunkLen, data + off, 4);
        std::memcpy(&chunkType, data + off + 4, 4);
        off += 8;
        if (off + chunkLen > len) {
            err = "GLB: chunk truncado";
            return false;
        }
        if (chunkType == kChunkJson && !jsonPtr) {
            jsonPtr = data + off;
            jsonLen = chunkLen;
        } else if (chunkType == kChunkBin && !binFromChunk) {
            bin.assign(data + off, data + off + chunkLen);
            binFromChunk = true;
        }
        off += chunkLen;
    }

    if (!jsonPtr) {
        err = "GLB: sem chunk JSON";
        return false;
    }
    // o JSON chunk pode ter padding com espaços (alinhamento de 4) — o
    // parser do engine tolera whitespace no fim, ok direto
    return parseGltf(reinterpret_cast<const char*>(jsonPtr), jsonLen, bin,
                     resolver, out, err);
}

} // namespace vv
