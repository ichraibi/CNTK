# Copyright (c) Microsoft. All rights reserved.

# Licensed under the MIT license. See LICENSE.md file in the project root
# for full license information.
# ==============================================================================

from __future__ import print_function
import os
import numpy as np
from cntk import load_model
from cntk.ops import combine
from cntk.io import MinibatchSource, CTFDeserializer, StreamDef, StreamDefs, FULL_DATA_SWEEP
from PIL import Image
from cntk import graph


# Paths relative to current python file.
abs_path   = os.path.dirname(os.path.abspath(__file__))
data_path  = os.path.join(abs_path, "..", "DataSets", "MNIST")
model_path = os.path.join(abs_path, "Output", "Models")


# Helper to print all node names
def print_all_node_names(model_file, is_BrainScript=True):
    loaded_model = load_model(model_file)
    if is_BrainScript:
        loaded_model = combine([loaded_model.outputs[0]])
    node_list = graph.depth_first_search(loaded_model, lambda x: x.is_output)
    print("printing node information in the format")
    print("node name (tensor shape)")
    for node in node_list:
        print(node.name, node.shape)


# Helper to save array as grayscale image
def save_as_png(val_array, img_file_name, dim=28):
    img_array = val_array.reshape((dim, dim))
    img_array = np.clip(img_array, 0, img_array.max())
    img_array *= 255.0 / img_array.max()
    img_array = np.rint(img_array).astype('uint8')

    im = Image.fromarray(img_array)
    im2 = im.resize((224,224))
    im2.save(img_file_name)


if __name__ == '__main__':
    num_objects_to_eval = 5

    # define location of output, model and data and check existence
    output_path = os.path.join(abs_path, "Output")
    model_file = os.path.join(model_path, "07_Deconvolution.model")
    data_file = os.path.join(data_path, "Test-28x28_cntk_text.txt")
    if not (os.path.exists(model_file) and os.path.exists(data_file)):
        print("Cannot find required data or model. "
              "Please get the MNIST data set and run 'cntk configFile=07_Deconvolution.cnkt' to create the model.")
        exit(0)

    # create minibatch source
    minibatch_source = MinibatchSource(CTFDeserializer(data_file, StreamDefs(
        features  = StreamDef(field='features', shape=(28*28)),
        labels    = StreamDef(field='labels',   shape=10)
    )), randomize=False, epoch_size = FULL_DATA_SWEEP)

    # load model and pick desired nodes as output
    # print_all_node_names(model_file)
    loaded_model = load_model(model_file)
    output_nodes = combine(
        [loaded_model.find_by_name('f1').owner,
         loaded_model.find_by_name('z.p1').owner,
         loaded_model.find_by_name('z').owner])

    # evaluate model save output
    features_si = minibatch_source['features']
    with open(os.path.join(output_path, "decoder_output_py.txt"), 'wb') as decoder_text_file:
        with open(os.path.join(output_path, "encoder_output_py.txt"), 'wb') as encoder_text_file:
            for i in range(0, num_objects_to_eval):
                mb = minibatch_source.next_minibatch(1)
                raw_dict = output_nodes.eval(mb[features_si])
                output_dict = {}
                for key in raw_dict.keys(): output_dict[key.name] = raw_dict[key]

                encoder_input = output_dict['f1']
                encoder_output = output_dict['z.p1']
                decoder_output = output_dict['z']
                in_values = (encoder_input[0,0].flatten())[np.newaxis]
                enc_values = (encoder_output[0,0].flatten())[np.newaxis]
                out_values = (decoder_output[0,0].flatten())[np.newaxis]

                # write results as text and png
                np.savetxt(decoder_text_file, out_values, fmt="%.6f")
                np.savetxt(encoder_text_file, enc_values, fmt="%.6f")
                save_as_png(in_values,  os.path.join(output_path, "imageAutoEncoder_%s__input.png" % i))
                save_as_png(out_values, os.path.join(output_path, "imageAutoEncoder_%s_output.png" % i))

                # visualizing the encoding is only possible and meaningful with a single conv filter
                save_as_png(enc_values, os.path.join(output_path, "imageAutoEncoder_%s_encoding.png" % i), dim=7)

    print("Done. Wrote output to %s" % output_path)
