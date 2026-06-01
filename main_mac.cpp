/************************************************************************/
/*                                                                      */
/* (c) J. Fabrizio                                                      */
/*                                                                      */
/*                                                                      */
/************************************************************************/


#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <vector>

//#include <glt_transform.hh>

#include "object_vbo.hh"
#include "image.hh"
#include "image_io.hh"

//#define SAVE_RENDER

#define TEST_OPENGL_ERROR()                                                             \
  do {									\
    GLenum err = glGetError();					                        \
    if (err != GL_NO_ERROR) std::cerr << "OpenGL ERROR!" << __LINE__ << std::endl;      \
  } while(0)


GLuint teapot_vao_id;
GLuint quad_vao_id;
GLuint program_id;
GLint clickedLocation;
GLint clickedTimeLocation;

float clickTime = 0.0f;
bool clicked = false;

GLFWwindow *g_window = nullptr;

// ---- Render quality -------------------------------------------------------
// The heavy raymarch shader runs once per rendered pixel. To gain fluidity we
// render the scene into a lower-resolution offscreen framebuffer, then stretch
// it back onto the window. RENDER_SCALE is the quality knob:
//   1.0  = full resolution (sharpest, slowest)
//   0.5  = quarter the pixels (good balance)   <-- default
//   0.33 = ~9x fewer pixels (fastest, softest)
const float RENDER_SCALE = 0.5f;

GLuint fbo_id = 0;          // offscreen framebuffer
GLuint fbo_color_tex = 0;   // its color attachment
int render_width = 0;       // low-res render size
int render_height = 0;

static float now_seconds() {
  return static_cast<float>(glfwGetTime());
}

void mouseCallback(GLFWwindow *window, int button, int action, int mods)
{
  (void)window; (void)mods;
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && clickTime == 0.0)
  {
    clickTime = now_seconds();
    clicked = true;
  }
}

// (Re)create the low-res offscreen render target for a given window size.
void init_render_target(int win_w, int win_h) {
  render_width  = (int)(win_w * RENDER_SCALE);
  render_height = (int)(win_h * RENDER_SCALE);
  if (render_width  < 1) render_width  = 1;
  if (render_height < 1) render_height = 1;

  if (fbo_color_tex) glDeleteTextures(1, &fbo_color_tex);
  if (fbo_id)        glDeleteFramebuffers(1, &fbo_id);

  // Use a high texture unit so we don't clobber the scene samplers (units 0-2).
  glActiveTexture(GL_TEXTURE3);TEST_OPENGL_ERROR();
  glGenTextures(1, &fbo_color_tex);
  glBindTexture(GL_TEXTURE_2D, fbo_color_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, render_width, render_height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, nullptr);TEST_OPENGL_ERROR();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenFramebuffers(1, &fbo_id);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D, fbo_color_tex, 0);TEST_OPENGL_ERROR();
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cerr << "FAILURE: render target framebuffer incomplete" << std::endl;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  std::cout << "Render target: " << render_width << "x" << render_height
            << " (scale " << RENDER_SCALE << ")" << std::endl;
}

void window_resize(GLFWwindow *window, int width, int height) {
  (void)window;
  glViewport(0,0,width,height);TEST_OPENGL_ERROR();
  init_render_target(width, height);
}

void display() {
  GLint time_location = glGetUniformLocation(program_id, "uTime");
  glUniform1f(time_location, now_seconds());
  clickedLocation = glGetUniformLocation(program_id, "uClicked");
  glUniform1i(clickedLocation, clicked ? 1 : 0);
  clickedTimeLocation = glGetUniformLocation(program_id, "uClickTime");
  glUniform1f(clickedTimeLocation, clickTime);
  // Tell the shader the resolution it is actually being rasterized at.
  glUniform2f(glGetUniformLocation(program_id, "windowSize"),
              (float)render_width, (float)render_height);

  // 1) Render the scene into the low-res offscreen framebuffer.
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);TEST_OPENGL_ERROR();
  glViewport(0, 0, render_width, render_height);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);TEST_OPENGL_ERROR();
  glDisable(GL_DEPTH_TEST);
  glBindVertexArray(quad_vao_id);TEST_OPENGL_ERROR();
  glDrawArrays(GL_TRIANGLES, 0, 6);TEST_OPENGL_ERROR();
  glBindVertexArray(0);TEST_OPENGL_ERROR();
  glEnable(GL_DEPTH_TEST);

  // 2) Stretch (linear upscale) the low-res result onto the window.
  int win_w, win_h;
  glfwGetFramebufferSize(g_window, &win_w, &win_h);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_id);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glBlitFramebuffer(0, 0, render_width, render_height,
                    0, 0, win_w, win_h,
                    GL_COLOR_BUFFER_BIT, GL_LINEAR);TEST_OPENGL_ERROR();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  /*
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);TEST_OPENGL_ERROR();
  glBindVertexArray(teapot_vao_id);TEST_OPENGL_ERROR();
  glDrawArrays(GL_TRIANGLES, 0, vertex_buffer_data.size());TEST_OPENGL_ERROR();
  glBindVertexArray(0);TEST_OPENGL_ERROR();*/
#if defined(SAVE_RENDER)
  if (!saved) {
    tifo::rgb24_image *texture = new tifo::rgb24_image(800, 590);
    glReadPixels(150, 350, 800, 590, GL_RGB, GL_UNSIGNED_BYTE, texture->pixels);TEST_OPENGL_ERROR();
    //glReadPixels(0, 0, 1024, 1024, GL_RGB, GL_UNSIGNED_BYTE, texture->pixels);
    tifo::save_image(*texture, "render.tga");
    std::cout << "Save " << std::endl;
    delete texture;
    //saved = true;
  }
#endif
  glfwSwapBuffers(g_window);
}

bool init_glut(int &argc, char *argv[]) {
  (void)argc; (void)argv;
  if (!glfwInit()) {
    std::cerr << "Error while initializing GLFW" << std::endl;
    return false;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  g_window = glfwCreateWindow(1920, 1080, "Shader Programming", nullptr, nullptr);
  if (!g_window) {
    std::cerr << "Error while creating GLFW window" << std::endl;
    glfwTerminate();
    return false;
  }
  glfwMakeContextCurrent(g_window);
  glfwSetMouseButtonCallback(g_window, mouseCallback);
  glfwSetFramebufferSizeCallback(g_window, window_resize);
  return true;
}

bool init_glew() {
  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) {
    std::cerr << " Error while initializing glew";
    return false;
  }
  return true;
}

void init_GL() {
  glEnable(GL_DEPTH_TEST);TEST_OPENGL_ERROR();
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);TEST_OPENGL_ERROR();
  glEnable(GL_CULL_FACE);TEST_OPENGL_ERROR();
  glClearColor(0.4,0.4,0.4,1.0);TEST_OPENGL_ERROR();
  glPixelStorei(GL_UNPACK_ALIGNMENT,1);
  glPixelStorei(GL_PACK_ALIGNMENT,1);
}

void init_object_vbo_background() {
  float quad[] = {
    -1.0f, -1.0f, 0.0f,
    1.0f, -1.0f, 0.0f,
    1.0f, 1.0f, 0.0f,

    -1.0f, -1.0f, 0.0f,
    1.0f, 1.0f, 0.0f,
    -1.0f, 1.0f, 0.0f,
  };

  GLuint vbo_id;
  GLint vertex_location = glGetAttribLocation(program_id,"position");TEST_OPENGL_ERROR();

  glGenVertexArrays(1, &quad_vao_id);TEST_OPENGL_ERROR();
  glBindVertexArray(quad_vao_id);TEST_OPENGL_ERROR();

  glGenBuffers(1, &vbo_id);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
  glVertexAttribPointer(vertex_location, 3, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(vertex_location);

  glBindVertexArray(0);
}

void init_object_vbo() {
  int max_nb_vbo = 5;
  int nb_vbo = 0;
  int index_vbo = 0;
  GLuint vbo_ids[max_nb_vbo];

  GLint vertex_location = glGetAttribLocation(program_id,"position");TEST_OPENGL_ERROR();
  GLint normal_flat_location = glGetAttribLocation(program_id,"normalFlat");TEST_OPENGL_ERROR();
  GLint normal_smooth_location = glGetAttribLocation(program_id,"normalSmooth");TEST_OPENGL_ERROR();
  GLint color_location = glGetAttribLocation(program_id,"color");TEST_OPENGL_ERROR();
  GLint uv_location = glGetAttribLocation(program_id,"uv");TEST_OPENGL_ERROR();

  glGenVertexArrays(1, &teapot_vao_id);TEST_OPENGL_ERROR();
  glBindVertexArray(teapot_vao_id);TEST_OPENGL_ERROR();

  if (vertex_location!=-1) nb_vbo++;
  if (normal_flat_location!=-1) nb_vbo++;
  if (normal_smooth_location!=-1) nb_vbo++;
  if (color_location!=-1) nb_vbo++;
  if (uv_location!=-1) nb_vbo++;
  glGenBuffers(nb_vbo, vbo_ids);TEST_OPENGL_ERROR();

  if (vertex_location!=-1) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[index_vbo++]);TEST_OPENGL_ERROR();
    glBufferData(GL_ARRAY_BUFFER, vertex_buffer_data.size()*sizeof(float), vertex_buffer_data.data(), GL_STATIC_DRAW);TEST_OPENGL_ERROR();
    glVertexAttribPointer(vertex_location, 3, GL_FLOAT, GL_FALSE, 0, 0);TEST_OPENGL_ERROR();
    glEnableVertexAttribArray(vertex_location);TEST_OPENGL_ERROR();
  }

  if (normal_flat_location!=-1) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[index_vbo++]);TEST_OPENGL_ERROR();
    glBufferData(GL_ARRAY_BUFFER, normal_flat_buffer_data.size()*sizeof(float), normal_flat_buffer_data.data(), GL_STATIC_DRAW);TEST_OPENGL_ERROR();
    glVertexAttribPointer(normal_flat_location, 3, GL_FLOAT, GL_FALSE, 0, 0);TEST_OPENGL_ERROR();
    glEnableVertexAttribArray(normal_flat_location);TEST_OPENGL_ERROR();
  }

  if (normal_smooth_location!=-1) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[index_vbo++]);TEST_OPENGL_ERROR();
    glBufferData(GL_ARRAY_BUFFER, normal_smooth_buffer_data.size()*sizeof(float), normal_smooth_buffer_data.data(), GL_STATIC_DRAW);TEST_OPENGL_ERROR();
    glVertexAttribPointer(normal_smooth_location, 3, GL_FLOAT, GL_FALSE, 0, 0);TEST_OPENGL_ERROR();
    glEnableVertexAttribArray(normal_smooth_location);TEST_OPENGL_ERROR();
  }

  if (color_location!=-1) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[index_vbo++]);TEST_OPENGL_ERROR();
    glBufferData(GL_ARRAY_BUFFER, color_buffer_data.size()*sizeof(float), color_buffer_data.data(), GL_STATIC_DRAW);TEST_OPENGL_ERROR();
    glVertexAttribPointer(color_location, 3, GL_FLOAT, GL_FALSE, 0, 0);TEST_OPENGL_ERROR();
    glEnableVertexAttribArray(color_location);TEST_OPENGL_ERROR();
  }

  if (uv_location!=-1) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo_ids[index_vbo++]);TEST_OPENGL_ERROR();
    glBufferData(GL_ARRAY_BUFFER, uv_buffer_data.size()*sizeof(float), uv_buffer_data.data(), GL_STATIC_DRAW);TEST_OPENGL_ERROR();
    glVertexAttribPointer(uv_location, 2, GL_FLOAT, GL_FALSE, 0, 0);TEST_OPENGL_ERROR();
    glEnableVertexAttribArray(uv_location);TEST_OPENGL_ERROR();
  }

  glBindVertexArray(0);
}

void init_textures() {
  tifo::rgb24_image *texture = tifo::load_image("texture.tga");
  tifo::rgb24_image *lighting = tifo::load_image("lighting.tga");
  tifo::rgb24_image *normalmap = tifo::load_image("normalmap.tga");
  GLuint texture_id;
  GLuint lighting_id;
  GLuint normalmap_id;
  GLint tex_location;
  GLint light_location;
  GLint normalmap_location;

  std::cout << "texture " << texture->sx << " ," <<  texture->sy << "\n";
  std::cout << "light " << lighting->sx << " ," <<  lighting->sy << "\n";
  std::cout << "normalmap " << normalmap->sx << " ," <<  normalmap->sy << std::endl;

  GLint texture_units, combined_texture_units;
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texture_units);
  glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &combined_texture_units);
  std::cout << "Limit 1 " <<  texture_units << " limit 2 " << combined_texture_units << std::endl;

  glGenTextures(1, &texture_id);TEST_OPENGL_ERROR();
  glActiveTexture(GL_TEXTURE0);TEST_OPENGL_ERROR();
  glBindTexture(GL_TEXTURE_2D,texture_id);TEST_OPENGL_ERROR();
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texture->sx, texture->sy, 0, GL_RGB, GL_UNSIGNED_BYTE, texture->pixels);TEST_OPENGL_ERROR();
  tex_location = glGetUniformLocation(program_id, "texture_sampler");TEST_OPENGL_ERROR();
  std::cout << "tex_location " << tex_location << std::endl;
  glUniform1i(tex_location,0);TEST_OPENGL_ERROR();

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);TEST_OPENGL_ERROR();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);TEST_OPENGL_ERROR();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);TEST_OPENGL_ERROR();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);TEST_OPENGL_ERROR();

  glGenTextures(1, &lighting_id);TEST_OPENGL_ERROR();
  glActiveTexture(GL_TEXTURE1);TEST_OPENGL_ERROR();
  glBindTexture(GL_TEXTURE_2D,lighting_id);TEST_OPENGL_ERROR();
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, lighting->sx, lighting->sy, 0, GL_RGB, GL_UNSIGNED_BYTE, lighting->pixels);TEST_OPENGL_ERROR();
  light_location = glGetUniformLocation(program_id, "lighting_sampler");TEST_OPENGL_ERROR();
  std::cout << "light_location " << light_location << std::endl;
  glUniform1i(light_location,1);TEST_OPENGL_ERROR();


  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);TEST_OPENGL_ERROR();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);TEST_OPENGL_ERROR();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);TEST_OPENGL_ERROR();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);TEST_OPENGL_ERROR();

  glGenTextures(1, &normalmap_id);TEST_OPENGL_ERROR();
  glActiveTexture(GL_TEXTURE2);TEST_OPENGL_ERROR();
  glBindTexture(GL_TEXTURE_2D,normalmap_id);TEST_OPENGL_ERROR();
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, normalmap->sx, normalmap->sy, 0, GL_RGB, GL_UNSIGNED_BYTE, normalmap->pixels);TEST_OPENGL_ERROR();
  normalmap_location = glGetUniformLocation(program_id, "normalmap_sampler");TEST_OPENGL_ERROR();
  std::cout << "normalmap_location " << normalmap_location << std::endl;
  glUniform1i(normalmap_location,2);TEST_OPENGL_ERROR();


  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);TEST_OPENGL_ERROR();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);TEST_OPENGL_ERROR();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);TEST_OPENGL_ERROR();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);TEST_OPENGL_ERROR();

  delete texture;
  delete lighting;
  delete normalmap;
}

std::string load(const std::string &filename) {
  std::ifstream input_src_file(filename, std::ios::in);
  std::string ligne;
  std::string file_content="";
  if (input_src_file.fail()) {
    std::cerr << "FAILURE: can not load " << filename << "\n";
    return "";
  }
  while(getline(input_src_file, ligne)) {
    file_content = file_content + ligne + "\n";
  }
  file_content += '\0';
  input_src_file.close();
  return file_content;
}

bool load_and_compile_shader(const GLenum shader_type,const std::string shader_src_filename, GLuint &shader_id) {
  GLint compile_status = GL_TRUE;
  std::string shader_src = load(shader_src_filename);
  const GLchar *sources[1];
  sources[0] = shader_src.c_str();
  shader_id = glCreateShader(shader_type);TEST_OPENGL_ERROR();
  glShaderSource(shader_id, 1, sources, 0);TEST_OPENGL_ERROR();
  glCompileShader(shader_id);TEST_OPENGL_ERROR();
  glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compile_status);
  if(compile_status != GL_TRUE) {
      GLint log_size;
      char *shader_log;
      glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_size);
      shader_log = (char*)std::malloc(log_size+1); /* +1 pour le caractere de fin de chaine '\0' */
      if(shader_log != 0) {
      	glGetShaderInfoLog(shader_id, log_size, &log_size, shader_log);
	      std::cerr << "FAILURE can not compile shader " << shader_src_filename << ": " << shader_log << std::endl;
    	  std::free(shader_log);
      }
      glDeleteShader(shader_id);
      return false;
  }
  return true;
}


bool attach_and_link_program(const std::vector<GLuint> &shaders_id, GLuint &program_id) {
  GLint link_status=GL_TRUE;
  program_id=glCreateProgram();TEST_OPENGL_ERROR();
  if (program_id==0) return false;
  for(unsigned int i = 0 ; i < shaders_id.size() ; i++) {
    glAttachShader(program_id, shaders_id[i]);TEST_OPENGL_ERROR();
  }
  glLinkProgram(program_id);TEST_OPENGL_ERROR();
  glGetProgramiv(program_id, GL_LINK_STATUS, &link_status);
  if (link_status!=GL_TRUE) {
    GLint log_size;
    char *program_log;
    glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_size);
    program_log = (char*)std::malloc(log_size+1); /* +1 pour le caractere de fin de chaine '\0' */
    if(program_log != 0) {
      glGetProgramInfoLog(program_id, log_size, &log_size, program_log);
      std::cerr << "FAILURE: Program can not be linked " << program_log << std::endl;
      std::free(program_log);
    }
    for(unsigned int i = 0 ; i < shaders_id.size() ; i++) {
      glDetachShader(program_id, shaders_id[i]);TEST_OPENGL_ERROR();
    }
    glDeleteProgram(program_id);TEST_OPENGL_ERROR();
    program_id=0;
    return false;
  }
  //glUseProgram(program_id);TEST_OPENGL_ERROR();
  return true;
}


bool init_shaders() {
  GLuint vertex_shader_id, fragment_shader_id;
  if (!load_and_compile_shader(GL_VERTEX_SHADER, "vertex.shd", vertex_shader_id)) {
    return false;
  }
  if (!load_and_compile_shader(GL_FRAGMENT_SHADER, "fragment.shd", fragment_shader_id)) {
    return false;
  }
  std::vector<GLuint> shaders_id;
  shaders_id.push_back(vertex_shader_id);
  shaders_id.push_back(fragment_shader_id);
  if (!attach_and_link_program(shaders_id, program_id)) {
    for(unsigned int i = 0 ; i < shaders_id.size() ; i++) {
      glDeleteShader(shaders_id[i]);TEST_OPENGL_ERROR();
    }
    return false;
  }

  for(unsigned int i = 0 ; i < shaders_id.size() ; i++) {
    glDetachShader(program_id, shaders_id[i]);TEST_OPENGL_ERROR();
  }

  for(unsigned int i = 0 ; i < shaders_id.size() ; i++) {
    glDeleteShader(shaders_id[i]);TEST_OPENGL_ERROR();
  }
  glUseProgram(program_id);
  return true;
}

/*void tmp() {
  glt::matrix4 look = glt::matrix4::identity();
  glt::matrix4 frustum = glt::matrix4::identity();

  glt::frustum(frustum,
	       -1, 1, -1, 1,
	       5, 50000
	     );

  glt::look_at(look,
	       20, 20, 20,
  	       0, 0, 0,
  	       0, 1, 0
  	       );

  std::cout << "Look\n" << look << "\n";
  std::cout << "frustum\n" << frustum << std::endl;
  }*/

int main(int argc, char *argv[]) {
  if (!init_glut(argc, argv))
    std::exit(-1);
  if (!init_glew())
    std::exit(-1);
  init_GL();
  init_shaders();
  init_object_vbo();
  init_object_vbo_background();
  init_textures();

  int fb_width, fb_height;
  glfwGetFramebufferSize(g_window, &fb_width, &fb_height);
  glViewport(0, 0, fb_width, fb_height);
  init_render_target(fb_width, fb_height);

  while (!glfwWindowShouldClose(g_window)) {
    display();
    glfwPollEvents();
  }
  glfwTerminate();
  return 0;
}
