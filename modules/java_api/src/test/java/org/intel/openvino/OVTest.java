package org.intel.openvino;

import org.junit.Ignore;
import org.junit.Rule;
import org.junit.rules.TestWatcher;
import org.junit.runner.Description;

import java.nio.file.Paths;

@Ignore
public class OVTest {
    String modelXml;
    String modelBin;
    String modelOnnx;
    static String device;

    public OVTest() {

        device = System.getProperty("device");
        if (device == null || device.isEmpty()) {
            System.err.println("No device specified");
            System.exit(1);
        }

        modelXml =
                Paths.get(
                                System.getProperty("MODELS_PATH"),
                                "models",
                                "test_model",
                                "test_model_fp32.xml")
                        .toString();
        modelBin =
                Paths.get(
                                System.getProperty("MODELS_PATH"),
                                "models",
                                "test_model",
                                "test_model_fp32.bin")
                        .toString();

        modelOnnx =
                Paths.get(
                                System.getProperty("MODELS_PATH"),
                                "models",
                                "test_model",
                                "test_model.onnx")
                        .toString();
    }

    @Rule
    public TestWatcher watchman =
            new TestWatcher() {
                @Override
                protected void succeeded(Description description) {
                    System.out.println(description + "- [" + device + "] - OK");
                }

                @Override
                protected void failed(Throwable e, Description description) {
                    System.out.println(description + "- [" + device + "] - FAILED");
                }
            };
}
