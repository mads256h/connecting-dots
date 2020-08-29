#include <iostream>
#include <memory>
#include <chrono>

#include <cmath>


#include <pulse/pulseaudio.h>

static pa_stream *stream = nullptr;

static const pa_channel_map *map = nullptr;

float level = 0.0F;

static void stream_read_callback(pa_stream *s, size_t size, void *)
{

  const void *p = nullptr;

  if (pa_stream_peek(s, &p, &size) < 0) {
    std::cerr << "pa_stream_peak() failed\n";
    return;
  }

  const float *d = reinterpret_cast<const float *>(p);
  size_t samples = size / sizeof(float);
  const uint8_t nchan = map->channels;

  float levels[PA_CHANNELS_MAX];

  for (uint8_t c = 0; c < nchan; c++) levels[c] = 0;

  while (samples >= nchan) {
    for (uint8_t c = 0; c < nchan; c++) {
      const float v = std::fabs(d[c]);
      if (v > levels[c]) levels[c] = v;
    }

    d += nchan;
    samples -= nchan;
  }

  float new_level = 0.0F;
  int div = 0;
  for (uint8_t c = 0; c < nchan; c++) {
    new_level += levels[c];

    if (levels[c] != 0.0F) div++;
  }

  if (div == 0) div = 1;

  new_level = new_level / static_cast<float>(div);

  if (new_level > level) level = new_level;

  pa_stream_drop(s);
}


static void stream_state_callback(pa_stream *s, void *)
{
  switch (pa_stream_get_state(s)) {
  case PA_STREAM_UNCONNECTED:
    std::cout << "PA_STREAM_UNCONNECTED\n";
    break;

  case PA_STREAM_CREATING:
    std::cout << "PA_STREAM_CREATING\n";
    break;

  case PA_STREAM_READY:
    std::cout << "PA_STREAM_READY\n";
    map = pa_stream_get_channel_map(s);


    break;

  case PA_STREAM_FAILED:
    std::cout << "PA_STREAM_FAILED\n";
    break;

  case PA_STREAM_TERMINATED:
    std::cout << "PA_STREAM_TERMINATED\n";
    break;
  }
}


static void create_stream(pa_context *c,
  const char *name,
  const char * /* unused */,
  const pa_sample_spec &ss,
  const pa_channel_map &cmap)
{
  std::cout << "Creating stream on device: " << name << '\n';

  pa_sample_spec nss;

  nss.format = PA_SAMPLE_FLOAT32;
  nss.rate = ss.rate;
  nss.channels = ss.channels;

  stream = pa_stream_new(c, "Connecting dots", &nss, &cmap);
  pa_stream_set_state_callback(stream, stream_state_callback, nullptr);
  pa_stream_set_read_callback(stream, stream_read_callback, nullptr);
  pa_stream_connect_record(stream, name, nullptr, PA_STREAM_PEAK_DETECT);
}


static void
  context_get_sink_info_callback(pa_context *c, const pa_sink_info *si, int is_last, void *)
{
  if (is_last < 0) {
    std::cerr << "Failed to get sink information\n";
    return;
  }

  if (!si) return;

  create_stream(c, si->monitor_source_name, si->description, si->sample_spec, si->channel_map);
}


static void context_get_server_info_callback(pa_context *c, const pa_server_info *si, void *)
{
  if (!si) {
    std::cerr << "Failed to get server information\n";
    return;
  }

  if (!si->default_sink_name) {
    std::cerr << "No default sink set.\n";
    return;
  }

  pa_operation_unref(pa_context_get_sink_info_by_name(
    c, si->default_sink_name, context_get_sink_info_callback, nullptr));
}

void context_state_callback(pa_context *c, void *)
{
  switch (pa_context_get_state(c)) {
  case PA_CONTEXT_UNCONNECTED:
    std::cout << "PA_CONTEXT_UNCONNECTED\n";
    break;
  case PA_CONTEXT_CONNECTING:
    std::cout << "PA_CONTEXT_CONNECTING\n";
    break;
  case PA_CONTEXT_AUTHORIZING:
    std::cout << "PA_CONTEXT_AUTHORIZING\n";
    break;
  case PA_CONTEXT_SETTING_NAME:
    std::cout << "PA_CONTEXT_SETTING_NAME\n";
    break;
  case PA_CONTEXT_READY:
    std::cout << "PA_CONTEXT_READY\n";
    pa_operation_unref(pa_context_get_server_info(c, context_get_server_info_callback, nullptr));
    break;
  case PA_CONTEXT_FAILED:
    std::cout << "PA_CONTEXT_FAILED\n";
    break;
  case PA_CONTEXT_TERMINATED:
    std::cout << "PA_CONTEXT_TERMINATED\n";
    break;
  default:
    std::cout << "WTF? " << pa_context_get_state(c) << '\n';
  }
}
