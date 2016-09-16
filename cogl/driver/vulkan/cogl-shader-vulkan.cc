/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2016 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 */

extern "C" {

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-context-private.h"
#include "cogl-debug.h"
#include "cogl-driver-vulkan-private.h"
#include "cogl-shader-vulkan-private.h"
#include "cogl-util-vulkan-private.h"

}

#include "glslang/InitializeDll.h"
#include "glslang/glslang/Public/ShaderLang.h"
#include "glslang/glslang/MachineIndependent/localintermediate.h"

#include "GlslangToSpv.h"
#include "disassemble.h"

#include <sstream>
#include <vector>

struct _CoglShaderVulkan
{
  CoglContext *context;

  glslang::TProgram *program;

  GHashTable *inputs[COGL_SHADER_VULKAN_NB_STAGES];
  GHashTable *outputs[COGL_SHADER_VULKAN_NB_STAGES];

  GHashTable *samplers[COGL_SHADER_VULKAN_NB_STAGES];
  GHashTable *uniforms;

  uint32_t input_layout_offset[COGL_SHADER_VULKAN_NB_STAGES];
  uint32_t output_layout_offset[COGL_SHADER_VULKAN_NB_STAGES];

  VkShaderModule modules[COGL_SHADER_VULKAN_NB_STAGES];

  int block_size[COGL_SHADER_VULKAN_NB_STAGES];

  char *shaders[COGL_SHADER_VULKAN_NB_STAGES];
};

static CoglShaderVulkanAttribute *
_cogl_shader_vulkan_attribute_new (const char *name)
{
  CoglShaderVulkanAttribute *attribute =
    g_slice_new0 (CoglShaderVulkanAttribute);

  attribute->name = g_strdup (name);

  return attribute;
}

static void
_cogl_shader_vulkan_attribute_free (CoglShaderVulkanAttribute *attribute)
{
  if (attribute->name)
    g_free (attribute->name);
  g_slice_free (CoglShaderVulkanAttribute, attribute);
}

static CoglShaderVulkanUniform *
_cogl_shader_vulkan_uniform_new (const char *name)
{
  CoglShaderVulkanUniform *uniform =
    g_slice_new0 (CoglShaderVulkanUniform);
  int i;

  uniform->name = g_strdup (name);
  for (i = 0; i < COGL_SHADER_VULKAN_NB_STAGES; i++)
    uniform->offsets[i] = -1;

  return uniform;
}

static void
_cogl_shader_vulkan_uniform_free (CoglShaderVulkanUniform *uniform)
{
  if (uniform->name)
    g_free (uniform->name);
  g_slice_free (CoglShaderVulkanUniform, uniform);
}

static CoglShaderVulkanSampler *
_cogl_shader_vulkan_sampler_new (const char *name, int binding)
{
  CoglShaderVulkanSampler *sampler =
    g_slice_new0 (CoglShaderVulkanSampler);

  sampler->name = g_strdup (name);
  sampler->binding = binding;

  return sampler;
}

static void
_cogl_shader_vulkan_sampler_free (CoglShaderVulkanSampler *sampler)
{
  if (sampler->name)
    g_free (sampler->name);
  g_slice_free (CoglShaderVulkanSampler, sampler);
}

static EShLanguage
_cogl_glsl_shader_type_to_es_language (CoglGlslShaderType type)
{
  switch (type)
    {
    case COGL_GLSL_SHADER_TYPE_VERTEX:
      return EShLangVertex;
    case COGL_GLSL_SHADER_TYPE_FRAGMENT:
      return EShLangFragment;
    default:
      g_assert_not_reached();
    }
}

/* These numbers come from the OpenGL 4.4 core profile specification Chapter
   23 unless otherwise specified. */
const TBuiltInResource kDefaultTBuiltInResource = {
  /*.maxLights = */ 8,         /* From OpenGL 3.0 table 6.46. */
  /*.maxClipPlanes = */ 6,     /* From OpenGL 3.0 table 6.46. */
  /*.maxTextureUnits = */ 2,   /* From OpenGL 3.0 table 6.50. */
  /*.maxTextureCoords = */ 8,  /* From OpenGL 3.0 table 6.50. */
  /*.maxVertexAttribs = */ 16,
  /*.maxVertexUniformComponents = */ 4096,
  /*.maxVaryingFloats = */ 60,  /* From OpenGLES 3.1 table 6.44. */
  /*.maxVertexTextureImageUnits = */ 16,
  /*.maxCombinedTextureImageUnits = */ 80,
  /*.maxTextureImageUnits = */ 16,
  /*.maxFragmentUniformComponents = */ 1024,
  /*.maxDrawBuffers = */ 2,
  /*.maxVertexUniformVectors = */ 256,
  /*.maxVaryingVectors = */ 15,  /* From OpenGLES 3.1 table 6.44. */
  /*.maxFragmentUniformVectors = */ 256,
  /*.maxVertexOutputVectors = */ 16,   /* maxVertexOutputComponents / 4 */
  /*.maxFragmentInputVectors = */ 15,  /* maxFragmentInputComponents / 4 */
  /*.minProgramTexelOffset = */ -8,
  /*.maxProgramTexelOffset = */ 7,
  /*.maxClipDistances = */ 8,
  /*.maxComputeWorkGroupCountX = */ 65535,
  /*.maxComputeWorkGroupCountY = */ 65535,
  /*.maxComputeWorkGroupCountZ = */ 65535,
  /*.maxComputeWorkGroupSizeX = */ 1024,
  /*.maxComputeWorkGroupSizeX = */ 1024,
  /*.maxComputeWorkGroupSizeZ = */ 64,
  /*.maxComputeUniformComponents = */ 512,
  /*.maxComputeTextureImageUnits = */ 16,
  /*.maxComputeImageUniforms = */ 8,
  /*.maxComputeAtomicCounters = */ 8,
  /*.maxComputeAtomicCounterBuffers = */ 1,  /* From OpenGLES 3.1 Table 6.43 */
  /*.maxVaryingComponents = */ 60,
  /*.maxVertexOutputComponents = */ 64,
  /*.maxGeometryInputComponents = */ 64,
  /*.maxGeometryOutputComponents = */ 128,
  /*.maxFragmentInputComponents = */ 128,
  /*.maxImageUnits = */ 8,  /* This does not seem to be defined anywhere,
    set to ImageUnits. */
  /*.maxCombinedImageUnitsAndFragmentOutputs = */ 8,
  /*.maxCombinedShaderOutputResources = */ 8,
  /*.maxImageSamples = */ 0,
  /*.maxVertexImageUniforms = */ 0,
  /*.maxTessControlImageUniforms = */ 0,
  /*.maxTessEvaluationImageUniforms = */ 0,
  /*.maxGeometryImageUniforms = */ 0,
  /*.maxFragmentImageUniforms = */ 8,
  /*.maxCombinedImageUniforms = */ 8,
  /*.maxGeometryTextureImageUnits = */ 16,
  /*.maxGeometryOutputVertices = */ 256,
  /*.maxGeometryTotalOutputComponents = */ 1024,
  /*.maxGeometryUniformComponents = */ 512,
  /*.maxGeometryVaryingComponents = */ 60,  /* Does not seem to be defined
   anywhere, set equal to maxVaryingComponents. */
  /*.maxTessControlInputComponents = */ 128,
  /*.maxTessControlOutputComponents = */ 128,
  /*.maxTessControlTextureImageUnits = */ 16,
  /*.maxTessControlUniformComponents = */ 1024,
  /*.maxTessControlTotalOutputComponents = */ 4096,
  /*.maxTessEvaluationInputComponents = */ 128,
  /*.maxTessEvaluationOutputComponents = */ 128,
  /*.maxTessEvaluationTextureImageUnits = */ 16,
  /*.maxTessEvaluationUniformComponents = */ 1024,
  /*.maxTessPatchComponents = */ 120,
  /*.maxPatchVertices = */ 32,
  /*.maxTessGenLevel = */ 64,
  /*.maxViewports = */ 16,
  /*.maxVertexAtomicCounters = */ 0,
  /*.maxTessControlAtomicCounters = */ 0,
  /*.maxTessEvaluationAtomicCounters = */ 0,
  /*.maxGeometryAtomicCounters = */ 0,
  /*.maxFragmentAtomicCounters = */ 8,
  /*.maxCombinedAtomicCounters = */ 8,
  /*.maxAtomicCounterBindings = */ 1,
  /*.maxVertexAtomicCounterBuffers = */ 0,  /* From OpenGLES 3.1 Table 6.41.
    ARB_shader_atomic_counters.*/
  /*.maxTessControlAtomicCounterBuffers = */ 0,
  /*.maxTessEvaluationAtomicCounterBuffers = */ 0,
  /*.maxGeometryAtomicCounterBuffers = */ 0,
  /* /ARB_shader_atomic_counters. */

  /*.maxFragmentAtomicCounterBuffers = */ 0,  /* From OpenGLES 3.1 Table 6.43. */
  /*.maxCombinedAtomicCounterBuffers = */ 1,
  /*.maxAtomicCounterBufferSize = */ 32,
  /*.maxTransformFeedbackBuffers = */ 4,
  /*.maxTransformFeedbackInterleavedComponents = */ 64,
  /*.maxCullDistances = */ 8,                 /* ARB_cull_distance. */
  /*.maxCombinedClipAndCullDistances = */ 8,  /* ARB_cull_distance. */
  /*.maxSamples = */ 4,
  /* This is the glslang TLimits structure.
     It defines whether or not the following features are enabled.
     We want them to all be enabled. */
  /*.limits = */ {
    /*.nonInductiveForLoops = */ 1,
    /*.whileLoops = */ 1,
    /*.doWhileLoops = */ 1,
    /*.generalUniformIndexing = */ 1,
    /*.generalAttributeMatrixVectorIndexing = */ 1,
    /*.generalVaryingIndexing = */ 1,
    /*.generalSamplerIndexing = */ 1,
    /*.generalVariableIndexing = */ 1,
    /*.generalConstantMatrixVectorIndexing = */ 1,
  }
};

extern "C" void
_cogl_shader_vulkan_free (CoglShaderVulkan *shader)
{
  CoglContextVulkan *vk_ctx = (CoglContextVulkan *) shader->context->winsys;

  for (int stage = 0; stage < COGL_SHADER_VULKAN_NB_STAGES; stage++) {
    g_hash_table_unref (shader->inputs[stage]);
    g_hash_table_unref (shader->outputs[stage]);
    g_hash_table_unref (shader->samplers[stage]);

    if (shader->modules[stage] != VK_NULL_HANDLE)
      VK ( shader->context,
           vkDestroyShaderModule (vk_ctx->device,
                                  shader->modules[stage], NULL) );

    if (shader->shaders[stage])
      g_free (shader->shaders[stage]);
  }

  g_hash_table_unref (shader->uniforms);

  if (shader->program)
    delete shader->program;
  g_slice_free (CoglShaderVulkan, shader);
}

extern "C" CoglShaderVulkan *
_cogl_shader_vulkan_new (CoglContext *context)
{
  CoglShaderVulkan *shader = g_slice_new0 (CoglShaderVulkan);

  glslang::InitializeProcess ();

  shader->context = context;
  shader->program = new glslang::TProgram();

  for (int stage = 0; stage < COGL_SHADER_VULKAN_NB_STAGES; stage++) {
    shader->inputs[stage] = g_hash_table_new_full (g_str_hash,
                                                   g_str_equal,
                                                   NULL,
                                                   (GDestroyNotify) _cogl_shader_vulkan_attribute_free);
    shader->outputs[stage] = g_hash_table_new_full (g_str_hash,
                                                    g_str_equal,
                                                    NULL,
                                                    (GDestroyNotify) _cogl_shader_vulkan_attribute_free);
    shader->samplers[stage] = g_hash_table_new_full (g_str_hash,
                                                     g_str_equal,
                                                     NULL,
                                                     (GDestroyNotify) _cogl_shader_vulkan_sampler_free);
  }

  shader->uniforms = g_hash_table_new_full (g_str_hash,
                                            g_str_equal,
                                            NULL,
                                            (GDestroyNotify) _cogl_shader_vulkan_uniform_free);


  return shader;
}

extern "C" void
_cogl_shader_vulkan_set_source (CoglShaderVulkan *shader,
                                CoglGlslShaderType stage,
                                const char *string)
{
  CoglContext *ctx = shader->context;
  glslang::TShader *gl_shader =
    new glslang::TShader (_cogl_glsl_shader_type_to_es_language (stage));
  gl_shader->setStrings(&string, 1);

  if (G_UNLIKELY (shader->shaders[stage]))
    g_free (shader->shaders[stage]);
  shader->shaders[stage] = g_strdup (string);

  glslang::TShader::ForbidInclude no_include;
  bool success = gl_shader->parse(&kDefaultTBuiltInResource,
                                  ctx->glsl_version_to_use,
                                  ENoProfile,
                                  false,
                                  false,
                                  static_cast<EShMessages>(EShMsgDefault |
                                                           EShMsgSpvRules |
                                                           EShMsgVulkanRules),
                                  no_include);

  if (!success)
    g_warning ("Shader compilation failed : %s\n%s\nShader:\n%s\n",
               gl_shader->getInfoLog(), gl_shader->getInfoDebugLog(), string);

  shader->program->addShader(gl_shader);
}

static void
_cogl_shader_vulkan_add_shader_input (CoglShaderVulkan *shader,
                                      CoglGlslShaderType stage,
                                      glslang::TIntermSymbol* symbol)
{
  CoglShaderVulkanAttribute *attribute =
    _cogl_shader_vulkan_attribute_new (symbol->getName().c_str());

  if (stage > COGL_GLSL_SHADER_TYPE_VERTEX) {
    CoglShaderVulkanAttribute *previous = (CoglShaderVulkanAttribute *)
      g_hash_table_lookup (shader->outputs[stage - 1], attribute->name);
    if (previous)
      symbol->getQualifier().layoutLocation = previous->location;
  } else {
    symbol->getQualifier().layoutLocation =
      shader->input_layout_offset[stage];

    glslang::TIntermediate* intermediate =
      shader->program->getIntermediate (_cogl_glsl_shader_type_to_es_language (stage));
    shader->input_layout_offset[stage] +=
      intermediate->computeTypeLocationSize(symbol->getType());
  }

  attribute->location = symbol->getQualifier().layoutLocation;

  g_hash_table_insert (shader->inputs[stage], attribute->name, attribute);
}

static void
_cogl_shader_vulkan_add_shader_output (CoglShaderVulkan *shader,
                                       CoglGlslShaderType stage,
                                       glslang::TIntermSymbol* symbol)
{
  CoglShaderVulkanAttribute *attribute =
    _cogl_shader_vulkan_attribute_new (symbol->getName().c_str());

  attribute->location =
    symbol->getQualifier().layoutLocation =
    shader->output_layout_offset[stage];

  g_hash_table_insert (shader->outputs[stage], attribute->name, attribute);

  glslang::TIntermediate* intermediate =
    shader->program->getIntermediate (_cogl_glsl_shader_type_to_es_language (stage));
  shader->output_layout_offset[stage] +=
    intermediate->computeTypeLocationSize(symbol->getType());
}

static void
_cogl_shader_vulkan_add_sampler (CoglShaderVulkan *shader,
                                 CoglGlslShaderType stage,
                                 glslang::TIntermSymbol* symbol)
{
  /* We start mapping samplers at 2 because 0 & 1 are used by uniform blocks
     (0 for vertex, 1 for fragment). */
  int binding = g_hash_table_size (shader->samplers[stage]) + 2;
  CoglShaderVulkanSampler *sampler =
    _cogl_shader_vulkan_sampler_new (symbol->getName().c_str(), binding);

  symbol->getQualifier().layoutBinding = sampler->binding;

  g_hash_table_insert (shader->samplers[stage], sampler->name, sampler);
}

static void
_cogl_shader_vulkan_add_uniform (CoglShaderVulkan *shader,
                                 CoglGlslShaderType stage,
                                 const char *name,
                                 int offset)
{
  CoglShaderVulkanUniform *uniform =
    (CoglShaderVulkanUniform *) g_hash_table_lookup (shader->uniforms, name);

  if (uniform == NULL)
    {
      uniform = _cogl_shader_vulkan_uniform_new (name);
      g_hash_table_insert (shader->uniforms, uniform->name, uniform);
    }

  uniform->offsets[stage] = offset;

}

/* Lookup or calculate the offset of a block member, using the recursively
   defined block offset rules. */
static int getOffset(glslang::TIntermediate* intermediate,
                     const glslang::TType& type,
                     int index)
{
  const glslang::TTypeList& memberList = *type.getStruct();

  /* Don't calculate offset if one is present, it could be user supplied and
     different than what would be calculated. That is, this is faster, but
     not just an optimization. */
  if (memberList[index].type->getQualifier().hasOffset())
    return memberList[index].type->getQualifier().layoutOffset;

  int memberSize;
  int dummyStride;
  int offset = 0;
  for (int m = 0; m <= index; ++m) {
    /* modify just the children's view of matrix layout, if there is one for
       this member */
    glslang::TLayoutMatrix subMatrixLayout =
      memberList[m].type->getQualifier().layoutMatrix;
    int memberAlignment =
      intermediate->getBaseAlignment(*memberList[m].type, memberSize,
                                     dummyStride,
                                     type.getQualifier().layoutPacking == glslang::ElpStd140,
                                     subMatrixLayout != glslang::ElmNone ? subMatrixLayout == glslang::ElmRowMajor : type.getQualifier().layoutMatrix == glslang::ElmRowMajor);
    glslang::RoundToPow2(offset, memberAlignment);
    if (m < index)
      offset += memberSize;
  }

  return offset;
}

static void
_cogl_shader_vulkan_add_block (CoglShaderVulkan *shader,
                               CoglGlslShaderType stage,
                               glslang::TIntermSymbol* symbol)
{
  const glslang::TType& block_type = symbol->getType();
  const glslang::TTypeList member_list = *block_type.getStruct();
  glslang::TIntermediate* intermediate =
    shader->program->getIntermediate (_cogl_glsl_shader_type_to_es_language (stage));

  symbol->getQualifier().layoutBinding = static_cast<int>(stage);

  int member_offset = 0;
  int member_size = 0;
  for (int i = 0; i < member_list.size(); i++) {
    const glslang::TType& member_type = *member_list[i].type;
    int dummy_stride;

    member_offset = getOffset (intermediate, block_type, i);
    intermediate->getBaseAlignment (member_type,
                                    member_size,
                                    dummy_stride,
                                    block_type.getQualifier().layoutPacking == glslang::ElpStd140,
                                    block_type.getQualifier().layoutMatrix == glslang::ElmRowMajor);

    /* TODO: We need a not so hacky way of accessing array elements. */
    if (member_type.isArray() &&
        member_type.isExplicitlySizedArray() &&
        member_type.getArraySizes()->getNumDims() == 1) {
      int nb_items = member_type.getArraySizes()->getDimSize(0);
      int item_offset = member_size / nb_items;

      for (int j = 0; j < nb_items; j++) {
        gchar* name_item =
          g_strdup_printf ("%s[%i]", member_type.getFieldName().c_str(), j);
        _cogl_shader_vulkan_add_uniform (shader,
                                         stage,
                                         name_item,
                                         member_offset + j * item_offset);
        g_free (name_item);
      }
    } else {
      _cogl_shader_vulkan_add_uniform (shader,
                                       stage,
                                       member_type.getFieldName().c_str(),
                                       member_offset);
    }
  }

  shader->block_size[stage] = member_offset + member_size;
}

typedef std::map<int, glslang::TIntermSymbol*> SymbolMap;

class CoglTraverser : public glslang::TIntermTraverser {
public:
  CoglTraverser(const SymbolMap& map) : map_(map) {}

private:
  void visitSymbol(glslang::TIntermSymbol* base) override {
    SymbolMap::const_iterator it = map_.find(base->getId());

    if (it != map_.end() && it->second != base) {
      COGL_NOTE (VULKAN,
                 "updating instance name=%s layout=%i->%i binding=%i->%i",
                 base->getName().c_str(),
                 base->getQualifier().layoutLocation,
                 it->second->getQualifier().layoutLocation,
                 base->getQualifier().layoutBinding,
                 it->second->getQualifier().layoutBinding);
      base->getQualifier().layoutLocation =
        it->second->getQualifier().layoutLocation;
      base->getQualifier().layoutBinding =
        it->second->getQualifier().layoutBinding;
    }
  }

  SymbolMap map_;
};

VkShaderModule
_cogl_shader_vulkan_build_shader_module (CoglShaderVulkan *shader,
                                         const glslang::TIntermediate *intermediate)
{
  CoglContextVulkan *vk_ctx = (CoglContextVulkan *) shader->context->winsys;

  std::vector<uint32_t> spirv;
  glslang::GlslangToSpv(*intermediate, spirv);

  if (spirv.empty())
    return VK_NULL_HANDLE;

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_SPIRV))) {
    std::stringstream spirv_dbg;
    spv::Disassemble(spirv_dbg, spirv);

    COGL_NOTE (SPIRV, "Spirv output size=%u bytes : \n%s",
               spirv.size() * sizeof (uint32_t),
               spirv_dbg.str().c_str());
  }

  VkShaderModuleCreateInfo info;
  memset (&info, 0, sizeof (info));
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = spirv.size() * sizeof (uint32_t);
  info.pCode = &spirv[0];

  VkShaderModule module;
  VK_RET_VAL ( shader->context,
               vkCreateShaderModule (vk_ctx->device, &info, NULL, &module),
               VK_NULL_HANDLE );

  return module;
}

extern "C" CoglBool
_cogl_shader_vulkan_link (CoglShaderVulkan *shader)
{
  if (!shader->program->link (static_cast<EShMessages>(EShMsgDefault))) {
    g_warning ("Cannot link program");
    return false;
  }

  for (int stage = 0; stage < COGL_SHADER_VULKAN_NB_STAGES; stage++) {
    EShLanguage lang =
      _cogl_glsl_shader_type_to_es_language ((CoglGlslShaderType) stage);
    glslang::TIntermediate* intermediate =
      shader->program->getIntermediate(lang);
    const glslang::TIntermSequence& globals =
      intermediate->getTreeRoot()->getAsAggregate()->getSequence();
    glslang::TIntermAggregate* linker_objects = nullptr;
    for (unsigned int f = 0; f < globals.size(); ++f) {
      glslang::TIntermAggregate* candidate = globals[f]->getAsAggregate();
      if (candidate && candidate->getOp() == glslang::EOpLinkerObjects) {
        linker_objects = candidate;
        break;
      }
    }

    if (!linker_objects) {
      g_warning ("Cannot find linker objects");
      return FALSE;
    }

    const glslang::TIntermSequence& global_vars = linker_objects->getSequence();
    SymbolMap updated_symbols;
    for (unsigned int f = 0; f < global_vars.size(); f++) {
      glslang::TIntermSymbol* symbol;
      if ((symbol = global_vars[f]->getAsSymbolNode())) {

        /* Ignore the builtin blocks like gl_PerVertex. */
        if (symbol->isStruct() && symbol->getQualifier().layoutBinding != 0)
          continue;

        std::pair<int, glslang::TIntermSymbol*> item(symbol->getId(), symbol);
        updated_symbols.insert(item);

        if (symbol->isStruct()) {
          _cogl_shader_vulkan_add_block (shader,
                                         (CoglGlslShaderType) stage,
                                         symbol);
        } else if (symbol->getQualifier().storage == glslang::EvqIn ||
                   symbol->getQualifier().storage == glslang::EvqInOut ||
                   symbol->getQualifier().storage == glslang::EvqVaryingIn) {
          _cogl_shader_vulkan_add_shader_input (shader,
                                                (CoglGlslShaderType) stage,
                                                symbol);
        } else if (symbol->getQualifier().storage == glslang::EvqOut ||
                   symbol->getQualifier().storage == glslang::EvqVaryingOut) {
          _cogl_shader_vulkan_add_shader_output (shader,
                                                 (CoglGlslShaderType) stage,
                                                 symbol);
        } else if (symbol->getBasicType() == glslang::EbtSampler) {
          _cogl_shader_vulkan_add_sampler (shader,
                                           (CoglGlslShaderType) stage,
                                           symbol);
        }
      }
    }

    /* Visit the AST to replace all occurences of nodes we might have
       changed. */
    CoglTraverser traverser(updated_symbols);
    intermediate->getTreeRoot()->traverse(&traverser);

    shader->modules[stage] =
      _cogl_shader_vulkan_build_shader_module (shader, intermediate);
  }

  /* Destroy the AST once we've extracted all useful informations from
     it. */
  delete shader->program;
  shader->program = nullptr;

  return true;
}

extern "C" CoglShaderVulkanUniform *
_cogl_shader_vulkan_get_uniform (CoglShaderVulkan *shader,
                                 const char *name)
{
  return (CoglShaderVulkanUniform *)
    g_hash_table_lookup (shader->uniforms, name);
}

extern "C" int
_cogl_shader_vulkan_get_uniform_block_size (CoglShaderVulkan *shader,
                                            CoglGlslShaderType stage,
                                            int index)
{
  return shader->block_size[stage];
}

extern "C" int
_cogl_shader_vulkan_get_input_attribute_location (CoglShaderVulkan *shader,
                                                  CoglGlslShaderType stage,
                                                  const char *name)
{
  CoglShaderVulkanAttribute *attribute =
    (CoglShaderVulkanAttribute *) g_hash_table_lookup (shader->inputs[stage],
                                                       name);

  if (attribute)
    return attribute->location;

  return -1;
}

extern "C" int
_cogl_shader_vulkan_get_sampler_binding (CoglShaderVulkan *shader,
                                         CoglGlslShaderType stage,
                                         const char *name)
{
  CoglShaderVulkanSampler *sampler =
    (CoglShaderVulkanSampler *) g_hash_table_lookup (shader->samplers[stage],
                                                     name);

  if (sampler)
    return sampler->binding;

  return -1;
}

extern "C" int
_cogl_shader_vulkan_get_n_samplers (CoglShaderVulkan *shader,
                                    CoglGlslShaderType stage)
{
  return g_hash_table_size (shader->samplers[stage]);
}

extern "C" VkShaderModule
_cogl_shader_vulkan_get_shader_module (CoglShaderVulkan *shader,
                                       CoglGlslShaderType stage)
{
  CoglContextVulkan *vk_ctx = (CoglContextVulkan *) shader->context->winsys;

  return shader->modules[stage];
}
