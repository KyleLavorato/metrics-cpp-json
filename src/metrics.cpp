#include <metrics/metric.h>

#include <atomic>
#include <vector>
#include <map>

namespace Metrics
{
    void ICounterValue::accept(IMetricVisitor& visitor) { visitor.visit(*this); }
    void IGaugeValue::accept(IMetricVisitor& visitor) { visitor.visit(*this); }
    void ISummary::accept(IMetricVisitor& visitor) { visitor.visit(*this); }
    void IHistogram::accept(IMetricVisitor& visitor) { visitor.visit(*this); }
    void IText::accept(IMetricVisitor& visitor) { visitor.visit(*this); }
    
    ICounterValue::~ICounterValue() { }
	IGaugeValue::~IGaugeValue() { }
	ISummary::~ISummary() { }
	IHistogram::~IHistogram() { }
    IText::~IText() { }

	class GaugeImpl : public IGaugeValue
	{
	private:
		std::atomic<double> m_value;

	public:
		GaugeImpl() { m_value.store(0.); };
		GaugeImpl(GaugeImpl&&) = delete;
		GaugeImpl(const GaugeImpl&) = delete;
		~GaugeImpl() = default;

		IGaugeValue& operator=(double value) override
		{
			m_value.store(value);
			return *this;
		};

		IGaugeValue& operator+=(double value) override
		{
			// TODO: revisit memory ordering
			double oldv = 0, newv = 0;
			do
			{
				oldv = m_value.load();
				newv = oldv + value;
			} while (!m_value.compare_exchange_weak(oldv, newv));
			return *this;
		};

		IGaugeValue& operator-=(double value) override
		{
			// TODO: revisit memory ordering
			double oldv = 0, newv = 0;
			do
			{
				oldv = m_value.load();
				newv = oldv - value;
			} while (!m_value.compare_exchange_weak(oldv, newv));
			return *this;
		};

		double value() const override
		{
			return m_value.load();
		}
	};

	class CounterImpl : public ICounterValue
	{
	private:
		std::atomic<uint64_t> m_value;

	public:
        CounterImpl() noexcept { m_value.store(0); };
        CounterImpl(uint64_t value) noexcept { m_value.store(value); };
		CounterImpl(CounterImpl&& other) = delete;
		CounterImpl(const CounterImpl&) = delete;
		~CounterImpl() = default;
		ICounterValue& operator++(int) override
		{
			m_value.fetch_add(1, std::memory_order_acq_rel);
			return *this;
		};
		ICounterValue& operator+=(uint64_t value) override
		{
			m_value.fetch_add(value, std::memory_order_acq_rel);
			return *this;
		};
		uint64_t value() const override
		{
			return m_value.load(std::memory_order_acquire);
		};
		void reset() override
		{
			m_value.store(0, std::memory_order_release);
		};
	};

	// Implements CKMS algorithm for approximate quantile calculation
	// Utilizes locking internally, so is the slowest of all metric classes
	class SummaryImpl : public ISummary {
	private:
		CounterImpl m_count;
		GaugeImpl m_sum;

	public:
        SummaryImpl(const std::vector<double>& quantiles, double error)
		{
		}

		SummaryImpl(const SummaryImpl&) = delete;

		ISummary& observe(double value) override {
			m_count++;
			m_sum += value;
			return *this;
		}

		std::vector<std::pair<double, uint64_t>> values() const override
		{
			std::vector<std::pair<double, uint64_t>> result;
			return result;
		};

		uint64_t count() const override { return m_count; };
		double sum() const override { return m_sum; };
	};

	class HistogramImpl : public IHistogram {
	private:
		struct Bucket {
			Bucket(double bound) : bound(bound) {};

			const double bound = 0.0;
			mutable CounterImpl counter;

			Bucket() = default;
			Bucket(Bucket&& other) noexcept :
				bound(other.bound),
				counter(other.counter.value())
			{
			}
			Bucket(const Bucket&) = delete;
		};

		const std::vector<Bucket> m_buckets;
		CounterImpl m_count;
		GaugeImpl m_sum;

		std::vector<Bucket> createBuckets(const std::vector<double>& bounds)
		{
			const auto size = bounds.size();
			std::vector<Bucket> result;
			result.reserve(size);
			for (auto bound : bounds)
				result.emplace_back(bound);
			return result;
		}

	public:
		HistogramImpl(const std::vector<double>& bounds) :
			m_buckets(createBuckets(bounds))
		{
		}

		HistogramImpl(const HistogramImpl&) = delete;

		IHistogram& observe(double value) override {
			for (const auto& bucket : m_buckets)
			{
				if (bucket.bound >= value)
					bucket.counter++;
			}
			m_count++;
			m_sum += value;
			return *this;
		}

		std::vector<std::pair<double, uint64_t>> values() const override
		{
			std::vector<std::pair<double, uint64_t>> result;
			result.reserve(m_buckets.size());
			for (const auto& bucket : m_buckets)
			{
				result.emplace_back(bucket.bound, bucket.counter.value());
			}
			return result;
		};

		uint64_t count() const override { return m_count; };
		double sum() const override { return m_sum; };
	};

    class TextImpl : public IText {
	private:
		std::string m_text;

	public:
        IText& operator=(std::string value)
		{
			m_text = value;
			return *this;
		};

        TextImpl(){};
		TextImpl(const TextImpl&) = delete;
        std::string value() const override { return m_text; };
	};

	// Definitions for functions referenced in registry.cpp
	std::shared_ptr<ICounterValue> makeCounter() { return std::make_shared<CounterImpl>(); };
	std::shared_ptr<IGaugeValue> makeGauge() { return std::make_shared<GaugeImpl>(); };
	std::shared_ptr<ISummary> makeSummary(const std::vector<double>& quantiles, double error) { return std::make_shared<SummaryImpl>(quantiles, error); };
	std::shared_ptr<IHistogram> makeHistogram(const std::vector<double>& bounds) { return std::make_shared<HistogramImpl>(bounds); };
    std::shared_ptr<IText> makeText() { return std::make_shared<TextImpl>(); };
}
