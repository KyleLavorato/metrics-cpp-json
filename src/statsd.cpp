#include <metrics/serialize.h>

#include <iostream>
#include <sstream>

using std::ostream;
using std::stringstream;
using std::string;
using std::endl;

namespace Metrics
{
    struct StatsdSerializer
    {
        stringstream os;

        string str() {
            return os.str();
        }

        void write(const Key& key)
        {
            os << key.name;
            for (auto kv = key.labels.cbegin(); kv != key.labels.cend(); kv++)
                os << "," << kv->first << "=" << kv->second;
        }

        void visit(const IRegistry& registry)
        {
            auto keys = registry.keys();
            for (const auto& key : keys)
            {
                auto metric = registry.get(key);
                if (!metric)
                    continue;

                switch (metric->type())
                {
                case TypeCode::Counter:
                    write(key);
                    os << "|" << std::static_pointer_cast<ICounterValue>(metric)->value() << "|c" << endl;
                    break;
                case TypeCode::Gauge:
                    write(key);
                    os << "|" << std::static_pointer_cast<IGaugeValue>(metric)->value() << "|g" << endl;
                    break;
                default:
                    break;
                }
            }
        }
    };

    std::string serializeStatsd(const IRegistry& registry)
    {
        StatsdSerializer s;
        s.visit(registry);
        return s.str();
    }
}
