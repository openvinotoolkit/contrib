package tests.compatibility;

import static org.junit.Assert.*;

import org.intel.openvino.compatibility.*;
import org.junit.Test;

import java.util.HashMap;
import java.util.Map;

public class IECoreTests extends IETest {
    IECore core = new IECore();

    @Test
    public void testReadNetwork() {
        CNNNetwork net = core.ReadNetwork(modelXml, modelBin);
        assertEquals("Network name", "test_model", net.getName());
    }

    @Test
    public void testReadNetworkXmlOnly() {
        CNNNetwork net = core.ReadNetwork(modelXml);
        assertEquals("Batch size", 1, net.getBatchSize());
    }

    @Test
    public void testReadNetworkIncorrectXmlPath() {
        String exceptionMessage = "";
        try {
            CNNNetwork net = core.ReadNetwork("model.xml", modelBin);
        } catch (Exception e) {
            exceptionMessage = e.getMessage();
        }
        assertFalse(exceptionMessage.isEmpty());
    }

    @Test
    public void testReadNetworkIncorrectBinPath() {
        String exceptionMessage = "";
        try {
            CNNNetwork net = core.ReadNetwork(modelXml, "model.bin");
        } catch (Exception e) {
            exceptionMessage = e.getMessage();
        }
        assertFalse(exceptionMessage.isEmpty());
    }

    @Test
    public void testLoadNetwork() {
        CNNNetwork net = core.ReadNetwork(modelXml, modelBin);
        ExecutableNetwork executableNetwork = core.LoadNetwork(net, device);

        assertTrue(executableNetwork instanceof ExecutableNetwork);
    }

    @Test
    public void testLoadNetworDeviceConfig() {
        CNNNetwork net = core.ReadNetwork(modelXml, modelBin);

        Map<String, String> testMap = new HashMap<String, String>();

        // When specifying key values as raw strings, omit the KEY_ prefix
        testMap.put("CPU_BIND_THREAD", "YES");
        testMap.put("CPU_THREADS_NUM", "1");

        ExecutableNetwork executableNetwork = core.LoadNetwork(net, device, testMap);

        assertTrue(executableNetwork instanceof ExecutableNetwork);
    }

    @Test
    public void testLoadNetworkWrongDevice() {
        String exceptionMessage = "";
        CNNNetwork net = core.ReadNetwork(modelXml, modelBin);
        try {
            core.LoadNetwork(net, "DEVICE");
        } catch (Exception e) {
            exceptionMessage = e.getMessage();
        }
        assertFalse(exceptionMessage.isEmpty());
    }
}
